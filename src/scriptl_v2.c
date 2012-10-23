/*
 * scriptlcom
 * Copyright (C) 2012  Ryan Pavlik
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public
 * License (LGPL) version 2.1 which accompanies this distribution, and
 * is available at http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "scriptl.h"

typedef void (*dispatch_fn_v2)(scl_config_t*, int);
#define dfn(fn) static void fn(scl_config_t *config, int fd)

typedef struct dispatch_table_v2 {
    char *name;
    dispatch_fn_v2 fn;
} dispatch_table_v2_t;

dfn(print_h);
dfn(error_h);
dfn(read_line_h);
dfn(read_bytes_h);

static dispatch_table_v2_t table_v2[] = {
    { ":print", print_h },
    { ":error", error_h },
    { ":read-line", read_line_h },
    { ":read-bytes", read_bytes_h },
    { 0, 0 }
};

void scl_write_cwd_v2(int fd) {
    char dirbuf[FILENAME_MAX+1] = { 0 };
    
    if(!getcwd(dirbuf, FILENAME_MAX+1)) {
        printf("scriptlcom: getcwd: path too long\n");
        exit(1);
    }

    scl_writef_packet(fd, "(:cwd #P\"%s/\")\n", dirbuf);
}

void scl_write_header_v2(scl_config_t *config, int fd) {
    int i;

    scl_write_packet(fd, "(:scriptl 2)", 0);
    scl_write_cwd_v2(fd);
    scl_writef_packet(fd, "(:funcall %s #P\"%s/\")",
                      config->function, config->script);
    scl_writef_packet(fd, "(:errors %s)",
                      config->errors ? config->errors : "nil");

    scl_writef_packet(fd, "(:args %d)", config->argc);
    for(i = 0; i < config->argc; i++)
        scl_write_packet(fd, config->argv[i], 0);
}

void scl_funcall2(scl_config_t *config, int fd) {
    char *packet = NULL;
    dispatch_table_v2_t *dispatch = NULL;
    int len = 0;
    
    scl_write_header_v2(config, fd);

    while((packet = scl_read_packet(fd, &len))) {
        int handled = 0;

        if(len == 0) {
            free(packet);
            return;
        }

        for(dispatch = table_v2; dispatch->name; dispatch++) {
            if(!strcmp(packet, dispatch->name)) {
                handled = 1;
                dispatch->fn(config, fd);
            }
        }

        if(!handled) {
            printf("scriptlcom: unhandled command: %s\n", packet);
            exit(1);
        }
        
        free(packet);
    }
}

dfn(print_h) {
    char *msg = scl_read_packet(fd, NULL);

    printf("%s", msg);
    free(msg);
}

dfn(error_h) {
    char *cond, *msg;

    cond = scl_read_packet(fd, NULL);
    msg  = scl_read_packet(fd, NULL);

    printf("Error: %s\n\n%s\n", cond, msg);
    exit(1);
}

dfn(read_line_h) {
    char *line = NULL;

    line = scl_read_line(STDIN_FILENO);
    scl_write_packet(fd, line, 0);
    free(line);
}

dfn(read_bytes_h) {
    char *count, *data;
    int c, bytes_read;

    count = scl_read_packet(fd, NULL);
    c = strtoul(count, NULL, 10);

    data = malloc(c);
    bytes_read = read(STDIN_FILENO, data, c);

    scl_write_packet(fd, data, bytes_read);
}
