#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/sys/printk.h>
#include <display/mb_display.h>

#define COMPANY_ID      0xFFFF
#define MFR_DATA_LEN    6   /* 2(company id) + 4(counter) */

static struct bt_le_per_adv_sync *pa_sync;
static struct mb_display *disp;
static bool sync_creating;

/* bt_data_parse() 콜백: Manufacturer Specific Data에서 카운터 추출 */
static bool parse_mfr_data(struct bt_data *data, void *user_data)
{
	uint32_t *out = user_data;

	if (data->type != BT_DATA_MANUFACTURER_DATA ||
	    data->data_len < MFR_DATA_LEN) {
		return true; /* 계속 파싱 */
	}

	uint16_t company_id = (uint16_t)data->data[0] |
			      ((uint16_t)data->data[1] << 8);
	if (company_id != COMPANY_ID) {
		return true;
	}

	*out = (uint32_t)data->data[2] |
	       ((uint32_t)data->data[3] << 8) |
	       ((uint32_t)data->data[4] << 16) |
	       ((uint32_t)data->data[5] << 24);

	return false; /* 파싱 완료 */
}

/* PA 동기화 완료 콜백 */
static void pa_synced(struct bt_le_per_adv_sync *sync,
		      struct bt_le_per_adv_sync_synced_info *info)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, addr, sizeof(addr));
	printk("Periodic Adv 동기화 완료! 브로드캐스터: %s\n", addr);

	/* 스캔 중단 (이미 동기화됨) */
	bt_le_scan_stop();

	mb_display_print(disp, MB_DISPLAY_MODE_SCROLL,
			 K_MSEC(300), "SYNC");
}

/* PA 동기화 해제 콜백 */
static void pa_term(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	printk("Periodic Adv 동기화 해제됨. 재스캔 시작...\n");
	pa_sync = NULL;
	sync_creating = false;

	mb_display_print(disp, MB_DISPLAY_MODE_SCROLL,
			 K_MSEC(300), "LOST");
}

/* PA 데이터 수신 콜백 - 브로드캐스터가 보낸 카운터 값 처리 */
static void pa_recv(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info,
		    struct net_buf_simple *buf)
{
	uint32_t counter = 0;

	bt_data_parse(buf, parse_mfr_data, &counter);

	printk("수신 카운터: %u  (RSSI: %d dBm)\n", counter, info->rssi);

	/* LED 매트릭스에 수신된 카운터 표시 */
	mb_display_print(disp, MB_DISPLAY_MODE_SCROLL,
			 K_MSEC(300), "%u", counter);
}

static struct bt_le_per_adv_sync_cb pa_sync_cb = {
	.synced = pa_synced,
	.term   = pa_term,
	.recv   = pa_recv,
};

/*
 * 스캔 수신 콜백
 * interval != 0 이면 해당 Extended Adv 장치가 Periodic Advertising을 지원함
 */
static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
	/* Periodic Advertising을 갖지 않는 장치 무시 */
	if (info->interval == 0) {
		return;
	}

	/* 이미 동기화 생성 중이거나 완료된 경우 무시 */
	if (sync_creating || pa_sync) {
		return;
	}

	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, addr, sizeof(addr));
	printk("PA 브로드캐스터 발견: %s (SID: %d, interval: %u * 1.25ms)\n",
	       addr, info->sid, info->interval);

	struct bt_le_per_adv_sync_param sync_param = {
		.addr    = *info->addr,
		.options = 0,
		.sid     = info->sid,
		.skip    = 0,
		.timeout = 0x00A0, /* 1600ms (160 * 10ms) */
	};

	int err = bt_le_per_adv_sync_create(&sync_param, &pa_sync);
	if (err) {
		printk("PA 동기화 생성 실패: %d\n", err);
		return;
	}

	sync_creating = true;
	printk("PA 동기화 생성 중...\n");
}

static struct bt_le_scan_cb scan_cb = {
	.recv = scan_recv,
};

static void start_scan(void)
{
	/*
	 * Extended Scanning 활성화
	 * CONFIG_BT_EXT_ADV=y 시 Extended Advertising 리포트도 수신됨
	 */
	struct bt_le_scan_param scan_param = {
		.type     = BT_LE_SCAN_TYPE_PASSIVE,
		.options  = BT_LE_SCAN_OPT_NONE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window   = BT_GAP_SCAN_FAST_WINDOW,
	};

	int err = bt_le_scan_start(&scan_param, NULL);
	if (err) {
		printk("스캔 시작 실패: %d\n", err);
	} else {
		printk("PA 브로드캐스터 스캔 중...\n");
	}
}

int main(void)
{
	int err;

	printk("=== BLE Periodic Advertising Receiver 시작 ===\n");

	disp = mb_display_get();
	mb_display_print(disp, MB_DISPLAY_MODE_SCROLL,
			 K_MSEC(300), "SCAN");

	/* Bluetooth 초기화 */
	err = bt_enable(NULL);
	if (err) {
		printk("BT 초기화 실패: %d\n", err);
		return err;
	}
	printk("Bluetooth 초기화 완료\n");

	bt_le_per_adv_sync_cb_register(&pa_sync_cb);
	bt_le_scan_cb_register(&scan_cb);

	start_scan();

	/*
	 * 메인 루프: 동기화가 끊어지면 재스캔
	 * 실제 카운터 표시는 pa_recv() 콜백에서 처리
	 */
	while (1) {
		k_sleep(K_SECONDS(5));

		if (!pa_sync && !sync_creating) {
			printk("동기화 없음 - 재스캔\n");
			mb_display_print(disp, MB_DISPLAY_MODE_SCROLL,
					 K_MSEC(300), "SCAN");
			start_scan();
		}
	}

	return 0;
}
