/*
 * micro:bit v2 Binary Clock with BLE Time Sync
 *
 * BLE 연결: nRF Connect 앱으로 "MBit-Clock" 검색 후 연결
 *           Time 특성에 3바이트 [시(0-23), 분(0-59), 초(0-59)] 쓰기
 *
 * LED 배치 (MSB 왼쪽, LSB 오른쪽):
 *   row 0: 시(hour)  — 5비트, col 0-4
 *   row 1: 분 십의 자리 — 3비트, col 2-4
 *   row 2: 분 일의 자리 — 4비트, col 1-4
 *   row 3: 초 십의 자리 — 3비트, col 2-4
 *   row 4: 초 일의 자리 — 4비트, col 1-4
 *
 * 버튼 A: 현재 시간 저장 (시리얼 출력)
 * 버튼 B: 하트 3초 표시
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <display/mb_display.h>

/* ─── BLE Custom Service UUIDs ──────────────────────────────────── */
/* Service:  AB120001-0000-0000-0000-000000000001                    */
/* Time Chr: AB120002-0000-0000-0000-000000000001                    */
#define BT_UUID_CLOCK_SVC_VAL \
	BT_UUID_128_ENCODE(0xAB120001, 0x0000, 0x0000, 0x0000, 0x000000000001ULL)
#define BT_UUID_TIME_CHR_VAL \
	BT_UUID_128_ENCODE(0xAB120002, 0x0000, 0x0000, 0x0000, 0x000000000001ULL)

static struct bt_uuid_128 clock_svc_uuid = BT_UUID_INIT_128(BT_UUID_CLOCK_SVC_VAL);
static struct bt_uuid_128 time_chr_uuid  = BT_UUID_INIT_128(BT_UUID_TIME_CHR_VAL);

/* ─── LED Display ───────────────────────────────────────────────── */
static struct mb_display *disp;

/*
 * 하트 패턴 (raw byte: bit4=col0 ... bit0=col4)
 *   . * . * .  → 0x0A
 *   * * * * *  → 0x1F
 *   * * * * *  → 0x1F
 *   . * * * .  → 0x0E
 *   . . * . .  → 0x04
 */
static const struct mb_image heart_img = {
	.row = { 0x0A, 0x1F, 0x1F, 0x0E, 0x04 }
};

/* ─── Time State ────────────────────────────────────────────────── */
static K_MUTEX_DEFINE(time_mtx);
static uint8_t cur_h, cur_m, cur_s;
static uint8_t saved_h, saved_m, saved_s;

static atomic_t heart_active = ATOMIC_INIT(0);

/* ─── Work Items ────────────────────────────────────────────────── */
static struct k_work_delayable heart_done_dwork;
static struct k_work            save_time_work;

/* ─── GPIO Buttons ──────────────────────────────────────────────── */
static const struct gpio_dt_spec btn_a = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec btn_b = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static struct gpio_callback btn_a_cb, btn_b_cb;

/* ─── Binary Clock Display ──────────────────────────────────────── */
/*
 * raw byte 인코딩: bit4=col0(좌), bit3=col1, bit2=col2, bit1=col3, bit0=col4(우)
 * 따라서 값 V를 오른쪽 정렬하면 row 바이트 = V & mask 그대로 사용 가능
 *
 * 예) 시=15(01111): row[0] = 15 & 0x1F = 0b01111 → _****
 *     분=47 → 십=4(100): row[1] = 4 & 0x07 = 0b100 → __*__  (col2=1)
 *              일=7(0111): row[2] = 7 & 0x0F = 0b0111 → _***
 */
static void show_clock(void)
{
	if (atomic_get(&heart_active)) {
		return;
	}

	uint8_t h, m, s;

	k_mutex_lock(&time_mtx, K_FOREVER);
	h = cur_h;
	m = cur_m;
	s = cur_s;
	k_mutex_unlock(&time_mtx);

	struct mb_image img = {
		.row = {
			[0] = h & 0x1F,
			[1] = (m / 10) & 0x07,
			[2] = (m % 10) & 0x0F,
			[3] = (s / 10) & 0x07,
			[4] = (s % 10) & 0x0F,
		}
	};

	mb_display_image(disp, MB_DISPLAY_MODE_SINGLE, K_FOREVER, &img, 1);
}

/* ─── Work Handlers ─────────────────────────────────────────────── */
static void save_time_fn(struct k_work *w)
{
	ARG_UNUSED(w);

	k_mutex_lock(&time_mtx, K_FOREVER);
	saved_h = cur_h;
	saved_m = cur_m;
	saved_s = cur_s;
	k_mutex_unlock(&time_mtx);

	printk("[버튼A] 저장된 시간: %02d:%02d:%02d\n", saved_h, saved_m, saved_s);
}

static void heart_done_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	atomic_set(&heart_active, 0);
	show_clock();
}

/* ─── Button ISR Callbacks ──────────────────────────────────────── */
static void on_btn_a(const struct device *dev, struct gpio_callback *cb,
		     uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_work_submit(&save_time_work);
}

static void on_btn_b(const struct device *dev, struct gpio_callback *cb,
		     uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	atomic_set(&heart_active, 1);
	mb_display_image(disp, MB_DISPLAY_MODE_SINGLE, K_SECONDS(3), &heart_img, 1);
	k_work_reschedule(&heart_done_dwork, K_SECONDS(3));
}

/* ─── BLE GATT Write Handler ────────────────────────────────────── */
static ssize_t write_time_chr(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len,
			      uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0 || len != 3) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	const uint8_t *d = (const uint8_t *)buf;

	if (d[0] > 23 || d[1] > 59 || d[2] > 59) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	k_mutex_lock(&time_mtx, K_FOREVER);
	cur_h = d[0];
	cur_m = d[1];
	cur_s = d[2];
	k_mutex_unlock(&time_mtx);

	printk("[BLE] 시간 수신: %02d:%02d:%02d\n", d[0], d[1], d[2]);
	return len;
}

/* ─── GATT Service Definition ───────────────────────────────────── */
BT_GATT_SERVICE_DEFINE(clock_svc,
	BT_GATT_PRIMARY_SERVICE(&clock_svc_uuid),
	BT_GATT_CHARACTERISTIC(&time_chr_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, write_time_chr, NULL),
);

/* ─── BLE Advertising ───────────────────────────────────────────── */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_NAME_COMPLETE,
		      'M', 'B', 'i', 't', '-', 'C', 'l', 'o', 'c', 'k'),
};

static void start_advertising(void)
{
	static const struct bt_le_adv_param param = BT_LE_ADV_PARAM_INIT(
		BT_LE_ADV_OPT_CONNECTABLE,
		BT_GAP_ADV_FAST_INT_MIN_2,
		BT_GAP_ADV_FAST_INT_MAX_2,
		NULL);

	int err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);

	if (err) {
		printk("광고 시작 실패: %d\n", err);
	} else {
		printk("BLE 광고 시작: 'MBit-Clock'\n");
	}
}

/* ─── BLE Connection Callbacks ──────────────────────────────────── */
static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("연결 실패 (err %u)\n", err);
		return;
	}
	printk("BLE 연결됨 — nRF Connect에서 시간 특성(Time Chr)에 "
	       "[시, 분, 초] 3바이트를 쓰세요\n");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	printk("BLE 연결 해제 (reason %u), 광고 재시작\n", reason);
	start_advertising();
}

BT_CONN_CB_DEFINE(conn_cbs) = {
	.connected    = on_connected,
	.disconnected = on_disconnected,
};

/* ─── Button Init ───────────────────────────────────────────────── */
static int buttons_init(void)
{
	if (!gpio_is_ready_dt(&btn_a) || !gpio_is_ready_dt(&btn_b)) {
		printk("GPIO 장치 준비 안 됨\n");
		return -ENODEV;
	}

	int err;

	err = gpio_pin_configure_dt(&btn_a, GPIO_INPUT);
	if (err) {
		return err;
	}
	err = gpio_pin_configure_dt(&btn_b, GPIO_INPUT);
	if (err) {
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&btn_a, GPIO_INT_EDGE_TO_ACTIVE);
	if (err) {
		return err;
	}
	err = gpio_pin_interrupt_configure_dt(&btn_b, GPIO_INT_EDGE_TO_ACTIVE);
	if (err) {
		return err;
	}

	gpio_init_callback(&btn_a_cb, on_btn_a, BIT(btn_a.pin));
	gpio_init_callback(&btn_b_cb, on_btn_b, BIT(btn_b.pin));
	gpio_add_callback(btn_a.port, &btn_a_cb);
	gpio_add_callback(btn_b.port, &btn_b_cb);

	return 0;
}

/* ─── Main ──────────────────────────────────────────────────────── */
int main(void)
{
	printk("=== micro:bit v2 Binary Clock ===\n");
	printk("BLE 이름: MBit-Clock\n");
	printk("시간 특성 UUID: AB120002-...\n");
	printk("쓰기 형식: [시(1바이트), 분(1바이트), 초(1바이트)]\n");

	disp = mb_display_get();

	k_work_init(&save_time_work, save_time_fn);
	k_work_init_delayable(&heart_done_dwork, heart_done_fn);

	int err = buttons_init();

	if (err) {
		printk("버튼 초기화 실패: %d\n", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("BT 초기화 실패: %d\n", err);
		return err;
	}
	printk("Bluetooth 초기화 완료\n");

	start_advertising();

	/* 초기 화면 표시 */
	show_clock();

	/* 1초마다 시간 증가 및 화면 갱신 */
	while (1) {
		k_sleep(K_SECONDS(1));

		k_mutex_lock(&time_mtx, K_FOREVER);
		if (++cur_s >= 60) {
			cur_s = 0;
			if (++cur_m >= 60) {
				cur_m = 0;
				if (++cur_h >= 24) {
					cur_h = 0;
				}
			}
		}
		k_mutex_unlock(&time_mtx);

		show_clock();
	}

	return 0;
}
