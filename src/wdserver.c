/**
 * wdserver.c -- An rfc6455-complaint Web Display Server 
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 FMSoft <http://www.minigui.com>
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "wdserver.h"
#include "log.h"
#include "xmalloc.h"
#include "websocket.h"

#include "unixsocket.h"

static WSServer *server = NULL;

/* *INDENT-OFF* */
static char short_options[] = "dp:Vh";
static struct option long_opts[] = {
  {"port"           , required_argument , 0 , 'p' } ,
  {"addr"           , required_argument , 0 ,  0  } ,
  {"echo-mode"      , no_argument       , 0 ,  0  } ,
  {"max-frame-size" , required_argument , 0 ,  0  } ,
  {"origin"         , required_argument , 0 ,  0  } ,
  {"prefix-path"    , required_argument , 0 ,  0  } ,
  {"prefix-url"     , required_argument , 0 ,  0  } ,
#if HAVE_LIBSSL
  {"ssl-cert"       , required_argument , 0 ,  0  } ,
  {"ssl-key"        , required_argument , 0 ,  0  } ,
#endif
  {"access-log"     , required_argument , 0 ,  0  } ,
  {"version"        , no_argument       , 0 , 'V' } ,
  {"help"           , no_argument       , 0 , 'h' } ,
  {0, 0, 0, 0}
};

/* Command line help. */
static void
cmd_help (void)
{
  printf ("\nWDServer - %s\n\n", WD_VERSION);

  printf (
  "Usage: "
  "wdserver [ options ... ] -p [--addr][--origin][...]\n"
  "The following options can also be supplied to the command:\n\n"
  ""
  "  -d                       - Run as a daemon.\n"
  "  -p --port=<port>         - Specifies the port to bind.\n"
  "  -h --help                - This help.\n"
  "  -V --version             - Display version information and exit.\n"
  "  --access-log=<path/file> - Specifies the path/file for the access log.\n"
  "  --addr=<addr>            - Specify an IP address to bind to.\n"
  "  --echo-mode              - Echo all received messages.\n"
  "  --max-frame-size=<bytes> - Maximum size of a websocket frame. This\n"
  "                             includes received frames from the client.\n"
  "  --origin=<origin>        - Ensure clients send the specified origin\n"
  "                             header upon the WebSocket handshake.\n"
  "  --prefix-path=<path>     - The path prefix to save the PNG files of dirty screen.\n"
  "  --prefix-url=<url>       - The URL prefix to fetch the PNG files for clients.\n"
  "  --ssl-cert=<cert.crt>    - Path to SSL certificate.\n"
  "  --ssl-key=<priv.key>     - Path to SSL private key.\n"
  "\n"
  "See the man page for more information `man wdserver`.\n\n"
  "For more details visit: http://www.minigui.com\n"
  "wdserver Copyright (C) 2018 by FMSoft\n"
  "\n"
  "wdserver is derived from gwsocket\n"
  "gwsocket Copyright (C) 2016 by Gerardo Orellana"
  "\n\n"
  );
}
/* *INDENT-ON* */

static void
handle_signal_action (int sig_number)
{
    if (sig_number == SIGINT) {
        printf ("SIGINT caught!\n");
        /* if it fails to write, force stop */
        ws_stop (server);
        _exit (1);
    }
    else if (sig_number == SIGPIPE) {
        printf ("SIGPIPE caught!\n");
    }
    else if (sig_number == SIGCHLD) {
        int pid;
        int status;

        while ((pid = waitpid (-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED (status)) {
                printf ("Child #%d exited with status: %x (return value: %d)\n", 
                        pid, status, WEXITSTATUS (status));
                ws_handle_buddy_exit (server, pid);
            }
            else if (WIFSIGNALED(status))
                printf ("Child #%d signaled by %d\n", pid, WTERMSIG (status));
        }
    }
}

static int
setup_signals (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = handle_signal_action;
  if (sigaction (SIGINT, &sa, 0) != 0) {
    perror ("sigaction()");
    return -1;
  }
  if (sigaction (SIGPIPE, &sa, 0) != 0) {
    perror ("sigaction()");
    return -1;
  }
  if (sigaction (SIGCHLD, &sa, 0) != 0) {
    perror ("sigaction()");
    return -1;
  }
  return 0;
}

static struct _demo_info {
    char* const demo_name;
    char* const working_dir;
    char* const exe_file;
    char* const def_mode;
} _demo_list [] = {
    {"mguxdemo", "/srv/devel/build-minigui-5.0/cell-phone-ux-demo", "/srv/devel/build-minigui-5.0/cell-phone-ux-demo/mguxdemo", "360x480-16bpp"},
    {"cbplusui", "/srv/devel/build-minigui-5.0/mg-demos/cbplusui/", "/srv/devel/build-minigui-5.0/mg-demos/cbplusui/cbplusui", "240x240-16bpp"},
};

/* return 0: bad request;
   return > 0: launched;
   return < 0: vfork error;
*/
static pid_t
wd_launch_client (const char* demo_name)
{
    int i, found = -1;
    pid_t pid = 0;

    for (i = 0; i < TABLESIZE (_demo_list); i++) {
        if (strcmp (_demo_list[i].demo_name, demo_name) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        return 0;
    }

    if ((pid = vfork ()) > 0) {
        ACCESS_LOG (("fork child for %s\n", demo_name));
    }
    else if (pid == 0) {
        int retval;
        char env_mode [32];

        retval = chdir (_demo_list[found].working_dir);
        if (retval)
            perror ("chdir");

        retval = wd_set_null_stdio ();
        if (retval)
            perror ("wd_set_null_stdio");
        
        strcpy (env_mode, "MG_DEFAULTMODE=");
        strcat (env_mode, _demo_list[found].def_mode);
        char *const argv[] = {_demo_list[found].demo_name, NULL};
        char *const envp[] = {"MG_GAL_ENGINE=usvfb", "MG_IAL_ENGINE=usvfb", env_mode, NULL};
        if (execve (_demo_list[found].exe_file, argv, envp) < 0)
			fprintf (stderr, "execve error\n");

        perror ("execl");
        _exit (1);
    }
    else {
        perror ("vfork");
        return -1;
    }

    return pid;
}

static pid_t
onopen (WSClient * client)
{
    printf ("INFO: Got a request from client (%d) %s and will launch a child\n", client->listener, client->headers->path);
    return wd_launch_client (client->headers->path + 1);
}

static int
onclose (WSClient * client)
{
    return 0;
}

static int
onmessage (WSClient * client)
{
    WSMessage **msg = &client->message;
    char* message = (*msg)->payload;
    struct _remote_event event = { EVENT_NULL };

    if (strncasecmp (message, "MOUSEDOWN ", 10) == 0) {
        if (sscanf (message + 10, "%d %d", &event.value1, &event.value2) == 2) {
            event.type = EVENT_LBUTTONDOWN;
        }
    }
    else if (strncasecmp (message, "MOUSEMOVE ", 10) == 0) {
        if (sscanf (message + 10, "%d %d", &event.value1, &event.value2) == 2) {
            event.type = EVENT_MOUSEMOVE;
        }
    }
    else if (strncasecmp (message, "MOUSEUP ", 8) == 0) {
        if (sscanf (message + 8, "%d %d", &event.value1, &event.value2) == 2) {
            event.type = EVENT_LBUTTONUP;
        }
    }
    else if (strncasecmp (message, "KEYDOWN ", 8) == 0) {
        if (sscanf (message + 8, "%d", &event.value1) == 1) {
            event.type = EVENT_KEYDOWN;
        }
    }
    else if (strncasecmp (message, "KEYUP ", 6) == 0) {
        if (sscanf (message + 6, "%d", &event.value1) == 1) {
            event.type = EVENT_KEYUP;
        }
    }

    if (event.type != EVENT_NULL) {
        us_send_event (client->us_buddy, &event);
    }
    else {
        LOG (("WARNING: got a unknown or bad message from client (%d): %s\n", client->listener, (*msg)->payload));
    }

    return 0;
}

static void
parse_long_opt (const char *name, const char *oarg)
{
  if (!strcmp ("echo-mode", name))
    ws_set_config_echomode (1);
  if (!strcmp ("max-frame-size", name))
    ws_set_config_frame_size (atoi (oarg));
  if (!strcmp ("origin", name))
    ws_set_config_origin (oarg);
  if (!strcmp ("unixsocket", name))
    ws_set_config_unixsocket (oarg);
  if (!strcmp ("access-log", name))
    ws_set_config_accesslog (oarg);
#if HAVE_LIBSSL
  if (!strcmp ("ssl-cert", name))
    ws_set_config_sslcert (oarg);
  if (!strcmp ("ssl-key", name))
    ws_set_config_sslkey (oarg);
#endif
  if (!strcmp ("prefix-path", name))
    ws_set_config_prefix_path (oarg);
  if (!strcmp ("prefix-url", name))
    ws_set_config_prefix_url (oarg);
}

/* Read the user's supplied command line options. */
static int
read_option_args (int argc, char **argv)
{
  int daemon = 0;
  int o, idx = 0;

  while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
    if (-1 == o || EOF == o)
      break;
    switch (o) {
    case 'd':
      daemon = 1;
      break;
    case 'p':
      ws_set_config_port (optarg);
      break;
    case 'h':
      cmd_help ();
      exit (EXIT_SUCCESS);
      return -1;
    case 'V':
      fprintf (stdout, "WDServer %s\n", WD_VERSION);
      exit (EXIT_SUCCESS);
      return -1;
    case 0:
      parse_long_opt (long_opts[idx].name, optarg);
      break;
    case '?':
      cmd_help ();
      exit (EXIT_SUCCESS);
      return -1;
    default:
      return -1;
    }
  }

  for (idx = optind; idx < argc; idx++)
    cmd_help ();

  return daemon;
}

int wd_set_null_stdio (void)
{
    int fd = open ("/dev/null", O_RDWR);
    if (fd < 0)
        return -1;

    if (dup2 (fd, 0) < 0 ||
            dup2 (fd, 1) < 0 ||
            dup2 (fd, 2) < 0) {
        close (fd);
        return -1;
    }

    close (fd);
    return 0;
}

int wd_daemon (void)
{
    pid_t pid;

    if (chdir ("/") != 0)
        return -1;

    if (wd_set_null_stdio ())
        return -1;

    pid = fork ();
    if (pid < 0)
        return -1;

    if (pid > 0)
        _exit(0);

    if (setsid () < 0)
        return -1;

    return 0;
}

int
main (int argc, char **argv)
{
    int retval;

    ws_set_config_host ("0.0.0.0");
    ws_set_config_port ("7788");
    ws_set_config_unixsocket (USS_PATH);
    ws_set_config_prefix_path (DEF_PREFIX_PATH);
    ws_set_config_prefix_url (DEF_PREFIX_URL);

    retval = read_option_args (argc, argv);
    if (retval >= 0) {
        if (retval && wd_daemon ()) {
            perror ("Error during wd_daemon");
            exit (EXIT_FAILURE);
        }

        setup_signals ();

        if ((server = ws_init ()) == NULL) {
            perror ("Error during ws_init");
            exit (EXIT_FAILURE);
        }

        /* callbacks */
        server->onclose = onclose;
        server->onmessage = onmessage;
        server->onopen = onopen;

        ws_start (server);
        ws_stop (server);
    }

    return EXIT_SUCCESS;
}
