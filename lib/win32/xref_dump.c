// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CLI definitions of a daemon, for vtysh.
 *
 * vtysh is built with the commands of every daemon. On ELF platforms
 * python/xrelfo.py reads them out of the daemons' binaries (the xref
 * records of DEFUN() and install_element()) into .xref files, which
 * python/xref2vtysh.py turns into vtysh_cmd.c. There is no such reader for
 * PE files, but a daemon has the same records in memory: when the build
 * runs it with FRR_XREF_DUMP set to a file name, it writes that .xref file
 * (its "cli" part) itself and exits before main().
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#include "command.h"
#include "json.h"
#include "xref.h"

static struct json_object *xref_dump_location(const struct xref *xref)
{
	struct json_object *loc = json_object_new_object();

	json_object_object_add(loc, "file", json_object_new_string(xref->file));
	json_object_object_add(loc, "line", json_object_new_int(xref->line));
	json_object_object_add(loc, "func", json_object_new_string(xref->func));
	return loc;
}

/* cli[<command name>][<binary>], filled from the DEFUN the first time */
static struct json_object *xref_dump_cmd(struct json_object *cli, const char *origin,
					 const struct cmd_element *cmd)
{
	static const struct {
		uint32_t attr;
		const char *name;
	} attrs[] = {
		{ CMD_ATTR_YANG, "yang" },
		{ CMD_ATTR_HIDDEN, "hidden" },
		{ CMD_ATTR_DEPRECATED, "deprecated" },
		{ CMD_ATTR_NOSH, "nosh" },
	};
	struct json_object *origins, *obj, *names;
	size_t i;

	if (!json_object_object_get_ex(cli, cmd->name, &origins)) {
		origins = json_object_new_object();
		json_object_object_add(cli, cmd->name, origins);
	}
	if (json_object_object_get_ex(origins, origin, &obj))
		return obj;

	obj = json_object_new_object();
	json_object_object_add(origins, origin, obj);
	json_object_object_add(obj, "string", json_object_new_string(cmd->string));
	json_object_object_add(obj, "doc", json_object_new_string(cmd->doc));
	if (cmd->attr) {
		names = json_object_new_array();
		for (i = 0; i < array_size(attrs); i++) {
			if (cmd->attr & attrs[i].attr)
				json_object_array_add(names, json_object_new_string(attrs[i].name));
		}
		json_object_object_add(obj, "attr", json_object_new_int(cmd->attr));
		json_object_object_add(obj, "attrs", names);
	}
	json_object_object_add(obj, "defun", xref_dump_location(&cmd->xref));
	return obj;
}

static void xref_dump(FILE *out, const char *origin)
{
	struct json_object *root = json_object_new_object();
	struct json_object *cli = json_object_new_object();
	const struct xref_block *block, *seen;
	const struct xref *const *xrefp;

	json_object_object_add(root, "refs", json_object_new_object());
	json_object_object_add(root, "cli", cli);

	for (block = xref_blocks; block; block = block->next) {
		/* libfrr is linked into the daemon: its block is the same */
		for (seen = xref_blocks; seen != block; seen = seen->next) {
			if (seen->start == block->start)
				break;
		}
		if (seen != block)
			continue;

		for (xrefp = block->start; xrefp < block->stop; xrefp++) {
			const struct xref_install_element *install;
			struct json_object *obj, *nodes, *node;
			const struct xref *xref = *xrefp;

			if (!xref)
				continue;

			switch (xref->type) {
			case XREFT_DEFUN:
				xref_dump_cmd(cli, origin,
					      container_of(xref, struct cmd_element, xref));
				break;
			case XREFT_INSTALL_ELEMENT:
				install = container_of(xref, struct xref_install_element, xref);
				obj = xref_dump_cmd(cli, origin, install->cmd_element);
				if (!json_object_object_get_ex(obj, "nodes", &nodes)) {
					nodes = json_object_new_array();
					json_object_object_add(obj, "nodes", nodes);
				}
				node = json_object_new_object();
				json_object_object_add(node, "node",
						       json_object_new_int(install->node_type));
				json_object_object_add(node, "install", xref_dump_location(xref));
				json_object_array_add(nodes, node);
				break;
			case XREFT_NONE:
			case XREFT_EVENTSCHED:
			case XREFT_LOGMSG:
			case XREFT_ASSERT:
				break;
			}
		}
	}

	fputs(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN), out);
	fputc('\n', out);
	json_object_put(root);
}

/* runs after the constructors with a priority, which register the xrefs */
static void __attribute__((constructor)) xref_dump_init(void)
{
	const char *path = getenv("FRR_XREF_DUMP");
	char exe[MAX_PATH], origin[2 * MAX_PATH];
	char *name, *dot;
	FILE *out;

	if (!path || !*path)
		return;

	/* python/xref2vtysh.py takes the daemon from "<directory>/<daemon>".
	 * It tells the commands of fabricd from the ones of isisd, which has
	 * the same sources, by their origin "isisd/fabricd".
	 */
	if (!GetModuleFileNameA(NULL, exe, sizeof(exe)))
		_exit(1);
	name = strrchr(exe, '\\');
	name = name ? name + 1 : exe;
	dot = strrchr(name, '.');
	if (dot)
		*dot = '\0';
	snprintf(origin, sizeof(origin), "%s/%s", strcmp(name, "fabricd") ? name : "isisd", name);

	out = fopen(path, "w");
	if (!out) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		_exit(1);
	}
	xref_dump(out, origin);
	if (fclose(out))
		_exit(1);
	_exit(0);
}
