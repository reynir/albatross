(* (c) 2017 Hannes Mehnert, all rights reserved *)

open Lwt.Infix

let rec read_tls_write_cons t =
  Vmm_tls_lwt.read_tls t >>= function
  | Error `Eof ->
    Logs.warn (fun m -> m "eof from server");
    Lwt.return Albatross_cli.Success
  | Error _ ->
    Lwt.return Albatross_cli.Communication_failed
  | Ok wire ->
    match Albatross_cli.output_result wire with
    | Ok () -> read_tls_write_cons t
    | Error e -> Lwt.return e

let client cas host port cert priv_key =
  Mirage_crypto_rng_lwt.initialize () >>= fun () ->
  let auth = if Sys.is_directory cas then `Ca_dir cas else `Ca_file cas in
  X509_lwt.authenticator auth >>= fun authenticator ->
  Lwt.catch (fun () ->
    (* TODO TLS certificate verification and gethostbyname:
       - allow IP address and hostname
       - if IP is specified, use it (and no TLS name verification - or SubjAltName with IP?)
       - if hostname is specified
         - no ip: gethostbyname
         - ip: connecto to ip and verify hostname *)
    Lwt_unix.gethostbyname host >>= fun host_entry ->
    let host_inet_addr = Array.get host_entry.Lwt_unix.h_addr_list 0 in
    let sockaddr = Lwt_unix.ADDR_INET (host_inet_addr, port) in
    Vmm_lwt.connect host_entry.Lwt_unix.h_addrtype sockaddr >>= function
    | None ->
      Logs.err (fun m -> m "couldn't connect to %a"
                   Vmm_lwt.pp_sockaddr sockaddr);
      Lwt.return Albatross_cli.Connect_failed
    | Some fd ->
      X509_lwt.private_of_pems ~cert ~priv_key >>= fun cert ->
      let certificates = `Single cert in
      let client = Tls.Config.client ~reneg:true ~certificates ~authenticator () in
      Tls_lwt.Unix.client_of_fd client (* ~host *) fd >>= fun t ->
      read_tls_write_cons t)
    (fun exn -> Lwt.return (Albatross_tls_common.classify_tls_error exn))

let run_client _ cas cert key (host, port) =
  Printexc.register_printer (function
      | Tls_lwt.Tls_alert x -> Some ("TLS alert: " ^ Tls.Packet.alert_type_to_string x)
      | Tls_lwt.Tls_failure f -> Some ("TLS failure: " ^ Tls.Engine.string_of_failure f)
      | _ -> None) ;
  Sys.(set_signal sigpipe Signal_ignore) ;
  Lwt_main.run (client cas host port cert key)

open Cmdliner
open Albatross_cli

let cas =
  let doc = "The full path to PEM encoded certificate authorities. Can either be a FILE or a DIRECTORY." in
  Arg.(required & pos 0 (some string) None & info [] ~doc ~docv:"CA")

let client_cert =
  let doc = "Use a client certificate chain" in
  Arg.(required & pos 1 (some file) None & info [] ~doc ~docv:"CERT")

let client_key =
  let doc = "Use a client key" in
  Arg.(required & pos 2 (some file) None & info [] ~doc ~docv:"KEY")

let destination =
  let doc = "the destination hostname:port to connect to" in
  Arg.(required & pos 3 (some host_port) None & info [] ~docv:"HOST:PORT" ~doc)

let cmd =
  let doc = "Albatross remote TLS client" in
  let man = [
    `S "DESCRIPTION" ;
    `P "$(tname) connects to an Albatross server and initiates a TLS handshake" ]
  in
  let exits = auth_exits @ exits in
  Term.(const run_client $ setup_log $ cas $ client_cert $ client_key $ destination),
  Term.info "albatross_client_remote_tls" ~version ~doc ~man ~exits

let () =
  match Term.eval cmd with
  | `Ok x -> exit (exit_status_to_int x)
  | y -> exit (Term.exit_status_of_result y)
