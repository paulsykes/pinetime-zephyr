/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <sys/byteorder.h>

static struct bt_conn *default_conn;

static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

static u8_t notify_func(struct bt_conn *conn,
		struct bt_gatt_subscribe_params *params,
		const void *data, u16_t length)
{
	printk("notify function \n");
	if (!data) {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	printk("[NOTIFICATION] data %p length %u\n", data, length);
	printk("heartbeat %d\n", *(u8_t *)(data+1));

	return BT_GATT_ITER_CONTINUE;
}

static u8_t discover_func(struct bt_conn *conn,
		const struct bt_gatt_attr *attr,
		struct bt_gatt_discover_params *params)
{
	int err;
	printk("Discover func\n");

	if (!attr) {
		printk("Discover complete\n");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	printk("[ATTRIBUTE] handle %u\n", attr->handle);
  //#define BT_UUID_CTS                       BT_UUID_DECLARE_16(0x1805)
  //        //#define BT_UUID_CTS_CURRENT_TIME          BT_UUID_DECLARE_16(0x2a2b)
	//if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HRS)) {
	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_CTS)) {
		//memcpy(&uuid, BT_UUID_HRS_MEASUREMENT, sizeof(uuid));
		memcpy(&uuid, BT_UUID_CTS_CURRENT_TIME, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				//BT_UUID_HRS_MEASUREMENT)) {
				BT_UUID_CTS_CURRENT_TIME)) {
		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else {
	//	subscribe_params.notify = notify_func;
	//	subscribe_params.value = BT_GATT_CCC_NOTIFY;
	//	subscribe_params.ccc_handle = attr->handle;
			printk("bt_gatt_subscribed\n");

		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			printk("Subscribe failed (err %d)\n", err);
		} else {
			printk("[SUBSCRIBED]\n");
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, u8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);
		return;
	}

	printk("Connected: %s\n", addr);

	if (conn == default_conn) {
		printk("conn = default\n");
		//memcpy(&uuid, BT_UUID_HRS, sizeof(uuid));
		memcpy(&uuid, BT_UUID_CTS, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = 0x0001;
		discover_params.end_handle = 0xffff;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			printk("Discover failed(err %d)\n", err);
			return;
		}
		else
			printk("Discover OK\n");

	}
	else
		printk("conn NOT default\n");

}

static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	int i;

	printk("[AD]: %u data_len %u\n", data->type, data->data_len);

	switch (data->type) {
		case BT_DATA_UUID16_SOME:
		case BT_DATA_UUID16_ALL:
			if (data->data_len % sizeof(u16_t) != 0U) {
				printk("AD malformed\n");
				return true;
			}
			else
				printk("AD wellformed\n");

			for (i = 0; i < data->data_len; i += sizeof(u16_t)) {
				struct bt_uuid *uuid;
				u16_t u16;
				int err;
				printk("i = %d\n",i);

				memcpy(&u16, &data->data[i], sizeof(u16));
				uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
				//if (bt_uuid_cmp(uuid, BT_UUID_HRS)) {
				if (bt_uuid_cmp(uuid, BT_UUID_CTS)) {
					printk("compare OK\n");
					continue;
				}
				else
					printk("compare NOT OK\n");

				err = bt_le_scan_stop();
				if (err) {
					printk("Stop LE scan failed (err %d)\n", err);
					continue;
				}

				default_conn = bt_conn_create_le(addr,
						BT_LE_CONN_PARAM_DEFAULT);
				return false;
			}
	}

	return true;
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
		struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
			dev, type, ad->len, rssi);

	/* We're only interested in connectable events */
	if (type == BT_LE_ADV_IND || type == BT_LE_ADV_DIRECT_IND) {
		bt_data_parse(ad, eir_found, (void *)addr);
	}
}

static int scan_start(void)
{
	/* Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime. */
	struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_ACTIVE,
		.filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	return bt_le_scan_start(&scan_param, device_found);
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	err = scan_start();
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

void main(void)
{
	int err;
	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);

	err = scan_start();

	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}
