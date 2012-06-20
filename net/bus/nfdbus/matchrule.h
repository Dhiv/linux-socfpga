/*
 * signals.h  Bus signal connection implementation
 *
 * Copyright (C) 2003  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef BUS_SIGNALS_H
#define BUS_SIGNALS_H

#include <linux/gfp.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <net/af_bus.h>

#include "message.h"

struct bus_match_rule *bus_match_rule_new(gfp_t gfp_flags);
void bus_match_rule_free(struct bus_match_rule *rule);

struct bus_match_rule *bus_match_rule_parse(const char *rule_text,
					    gfp_t gfp_flags);

struct rule_pool {
	/* Maps non-NULL interface names to a list of bus_match_rule */
	struct rb_root rules_by_iface;

	/* List of bus_match_rule which don't specify an interface */
	struct hlist_head rules_without_iface;
};

struct bus_match_maker {
	struct sockaddr_bus addr;

	struct hlist_node table_node;

	/* Pools of rules, grouped by the type of message they match. 0
	 * (DBUS_MESSAGE_TYPE_INVALID) represents rules that do not specify a
	 * message type.
	 */
	struct rule_pool rules_by_type[DBUS_NUM_MESSAGE_TYPES];

	struct rb_root names;

	struct kref kref;
};


struct bus_match_maker *bus_matchmaker_new(gfp_t gfp_flags);
void bus_matchmaker_free(struct kref *kref);

int bus_matchmaker_add_rule(struct bus_match_maker *matchmaker,
			    struct bus_match_rule *rule);
void bus_matchmaker_remove_rule_by_value(struct bus_match_maker *matchmaker,
					 struct bus_match_rule *value);

bool bus_matchmaker_filter(struct bus_match_maker *matchmaker,
			   struct bus_match_maker *sender,
			   int eavesdrop,
			   const struct dbus_message *message);

void bus_matchmaker_add_name(struct bus_match_maker *matchmaker,
			     const char *name, gfp_t gfp_flags);
void bus_matchmaker_remove_name(struct bus_match_maker *matchmaker,
				const char *name);

#endif /* BUS_SIGNALS_H */
