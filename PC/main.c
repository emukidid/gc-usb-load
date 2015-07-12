/*
 *  Copyright (C) 2008 dhewg, #wiidev efnet
 *
 *  this file is part of geckoloader
 *  http://wiibrew.org/index.php?title=Geckoloader
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gecko.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

unsigned char cmd_send = 0x80;

const char *envvar = "USBGECKODEVICE";

#ifndef __WIN32__
#ifdef __APPLE__
char *default_tty = "/dev/tty.usbserial-GECKUSB0";
#else
char *default_tty = "/dev/ttyUSB0";
#endif
#else
char *default_tty = NULL;
#endif

void wait_for_ack () {
        unsigned char ack;
        if (gecko_read (&ack, 1))
                fprintf (stderr, "\nerror receiving the ack\n");
        else if (ack != 0x08)
                fprintf (stderr, "\nunknown ack (0x%02x)\n", ack);
}

int main (int argc, char **argv) {
        int fd;
        struct stat st;
        char *tty;
        unsigned int size;
        unsigned char *buf, *p;
        off_t fsize, block;

        printf ("USB-Load 1.0 by emu_kidid\nUpload DOL files to a GameCube via USB-Gecko\n"
                "Based on geckoload code by dhewg\n\n");

        if (argc < 2) {
                fprintf (stderr, "usage: %s <file>\n\n", argv[0]);
                exit (EXIT_FAILURE);
        }

        tty = getenv (envvar);
        if (!tty)
                tty = default_tty;

        if (tty && stat (tty, &st))
                tty = NULL;

        if (!tty) {
                fprintf (stderr, "please set the environment variable %s to "
                         "your usbgecko "
#ifndef __WIN32__
                         "tty device (eg \"/dev/ttyUSB0\")"
#else
                         "COM port (eg \"COM3\")"
#endif
                         "\n", envvar);
                exit (EXIT_FAILURE);
        }

        printf ("using %s\n", tty);

        fd = open (argv[1], O_RDONLY | O_BINARY);
        if (fd < 0) {
                perror ("error opening the device");
                exit (EXIT_FAILURE);
        }

        if (fstat (fd, &st)) {
                close (fd);
                perror ("error stat'ing the file");
                exit (EXIT_FAILURE);
        }
        fsize = st.st_size;

        if (fsize < 1 || fsize > 16*1024*1024) {
                close (fd);
                fprintf (stderr, "error: invalid file size\n");
                exit (EXIT_FAILURE);
        }

        buf = malloc (fsize);
        if (!buf) {
                close (fd);
                fprintf (stderr, "out of memory\n");
                exit (EXIT_FAILURE);
        }

        if (read (fd, buf, fsize) != fsize) {
                close (fd);
                free (buf);
                perror ("error reading the file");
                exit (EXIT_FAILURE);
        }
        close (fd);

        if (gecko_open (tty)) {
                free (buf);
                fprintf (stderr, "unable to open the device\n");
                exit (EXIT_FAILURE);
        }

        printf ("sending upload request\n");
        if (gecko_write (&cmd_send, 1)) {
                free (buf);
                gecko_close ();
                exit (EXIT_FAILURE);
        }

        printf ("awaiting upload ack\n");
        wait_for_ack ();

        size = (unsigned int)fsize;

        printf ("sending file size (%u bytes)\n", (unsigned int) fsize);

        if (gecko_write (&size, 4)) {
                free (buf);
                gecko_close ();
                fprintf (stderr, "error sending data\n");
                exit (EXIT_FAILURE);
        }

        printf ("sending data");
        fflush (stdout);

        p = buf;
        while (fsize > 0) {
                block = fsize;
                if (block > 63448)
                        block = 63448;
                fsize -= block;

                if (gecko_write (p, block)) {
                        fprintf (stderr, "error sending block\n");
                        break;
                }
                p += block;

                printf (".");
                fflush (stdout);
        }

        printf ("done.\n");

        free (buf);
        gecko_close ();

        return 0;
}

