(*
 * Copyright (C) 2012      Citrix Ltd.
 * Author Ian Campbell <ian.campbell@citrix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only. with the special
 * exception on linking described in file LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *)

type level = Debug
	     | Verbose
	     | Detail
	     | Progress (* also used for "progress" messages *)
	     | Info
	     | Notice
	     | Warn
	     | Error
	     | Critical

val level_to_string : level -> string

type handle

(** call back arguments. See xentoollog.h for more info.
    vmessage:
      level: level as above
      errno: Some <errno> or None
      context: Some <string> or None
      message: The log message (already formatted)
    progress:
      context: Some <string> or None
      doing_what: string
      percent, done, total.
*)
type logger_cbs = {
		  vmessage : level -> int option -> string option -> string -> unit;
		  progress : string option -> string -> int -> int64 -> int64 -> unit;
		  (*destroy : handle -> unit*) }

external test: handle -> unit = "stub_xtl_test"

val create : string -> logger_cbs -> handle

val create_stdio_logger : ?level:level -> unit -> handle

external destroy: handle -> unit = "stub_xtl_destroy"
