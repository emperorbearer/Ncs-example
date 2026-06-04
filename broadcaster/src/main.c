#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/sys/printk.h>
#include <display/mb_display.h>

/* 테스트용 Company ID (0xFFFF) + 4바이트 카운터 */
#define COMPANY_ID_LO 0xFF
#define COMPANY_ID_HI 0xFF
#define MFR_DATA_LEN  6   /* 2(company id) + 4(counter) */

static struct bt_le_ext_adv *adv_set;
static struct mb_display *disp;
static uint32_t counter;

/* 카운터를 Manufacturer Specific Data로 인코딩하여 PA 데이터 업데이트 */
static int update_pa_data(uint32_t cnt)
{
	uint8_t mfr[MFR_DATA_LEN] = {
		COMPANY_ID_LO,
		COMPANY_ID_HI,
		(uint8_t)(cnt & 0xFF),
		(uint8_t)((cnt >> 8) & 0xFF),
		(uint8_t)((cnt >> 16) & 0xFF),
		(uint8_t)((cnt >> 24) & 0xFF),
	};

	struct bt_data pa_data[] = {
		BT_DATA(BT_DATA_MANUFACTURER_DATA, mfr, sizeof(mfr)),
	};

	int err = bt_le_per_adv_set_data(adv_set, pa_data, ARRAY_SIZE(pa_data));
	if (err) {
		printk("PA 데이터 업데이트 실패: %d\n", err);
	}
	return err;
}

int main(void)
{
	int err;

	printk("=== BLE Periodic Advertising Broadcaster 시작 ===\n");

	disp = mb_display_get();

	/* Bluetooth 초기화 */
	err = bt_enable(NULL);
	if (err) {
		printk("BT 초기화 실패: %d\n", err);
		return err;
	}
	printk("Bluetooth 초기화 완료\n");

	/*
	 * Extended Advertising 파라미터 설정
	 * Periodic Advertising은 Extended Advertising 위에서만 동작함
	 */
	struct bt_le_adv_param ext_adv_param =
		BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_EXT_ADV,
				     BT_GAP_ADV_FAST_INT_MIN_2,
				     BT_GAP_ADV_FAST_INT_MAX_2,
				     NULL);

	err = bt_le_ext_adv_create(&ext_adv_param, NULL, &adv_set);
	if (err) {
		printk("Extended Adv 세트 생성 실패: %d\n", err);
		return err;
	}

	/* Extended Advertising 데이터 (장치 이름 포함) */
	struct bt_data ext_adv_data[] = {
		BT_DATA(BT_DATA_FLAGS, (uint8_t[]){BT_LE_AD_NO_BREDR}, 1),
		BT_DATA_BYTES(BT_DATA_NAME_COMPLETE,
			      'M', 'B', 'i', 't', '-', 'P', 'A'),
	};

	err = bt_le_ext_adv_set_data(adv_set, ext_adv_data,
				     ARRAY_SIZE(ext_adv_data), NULL, 0);
	if (err) {
		printk("Extended Adv 데이터 설정 실패: %d\n", err);
		return err;
	}

	/*
	 * Periodic Advertising 파라미터 설정
	 * 인터벌: 최소 7.5ms ~ 최대 81.92s (단위: 1.25ms)
	 * BT_GAP_PER_ADV_FAST_INT_MIN_1 = 0x0018 (30ms)
	 * BT_GAP_PER_ADV_FAST_INT_MAX_1 = 0x0030 (60ms)
	 */
	err = bt_le_per_adv_set_param(adv_set, BT_LE_PER_ADV_DEFAULT);
	if (err) {
		printk("Periodic Adv 파라미터 설정 실패: %d\n", err);
		return err;
	}

	/* 초기 PA 데이터 설정 (카운터 = 0) */
	err = update_pa_data(counter);
	if (err) {
		return err;
	}

	/* Periodic Advertising 시작 (Extended Adv보다 먼저 시작해야 함) */
	err = bt_le_per_adv_start(adv_set);
	if (err) {
		printk("Periodic Adv 시작 실패: %d\n", err);
		return err;
	}

	/* Extended Advertising 시작 (수신기가 PA 동기화를 찾을 수 있도록) */
	err = bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Extended Adv 시작 실패: %d\n", err);
		return err;
	}

	printk("BLE Periodic Broadcasting 시작! 1초마다 카운터 증가\n");

	while (1) {
		printk("송신 카운터: %u\n", counter);

		/* LED 매트릭스에 카운터 표시 (스크롤) */
		mb_display_print(disp, MB_DISPLAY_MODE_SCROLL,
				 K_MSEC(300), "%u", counter);

		/* PA 데이터 업데이트 */
		update_pa_data(counter);

		counter++;
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
