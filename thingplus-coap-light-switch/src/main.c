/*
 * SparkFun Thing Plus Matter (MGM240P) - CoAP over Thread Light Switch
 *
 * 65mm 오락실 버튼(PB00, active-low)이 눌리면 Thread 메시로
 * CoAP NON PUT /<CONFIG_LIGHT_SWITCH_COAP_URI> (payload: "toggle")을 전송한다.
 * Border Router 호스트의 브리지(bridge/coap_matter_bridge.py)가 이를 받아
 * chip-tool로 Matter 전등의 OnOff 클러스터를 토글한다.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>

#include <openthread/coap.h>
#include <openthread/dataset.h>
#include <openthread/ip6.h>
#include <openthread/thread.h>

LOG_MODULE_REGISTER(coap_switch, LOG_LEVEL_INF);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static struct gpio_callback button_cb_data;
static int64_t last_press_ms;

static void send_toggle_handler(struct k_work *work);
static void led_off_handler(struct k_work *work);

static K_WORK_DEFINE(send_work, send_toggle_handler);
static K_WORK_DELAYABLE_DEFINE(led_off_work, led_off_handler);

static void led_off_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	gpio_pin_set_dt(&led, 0);
}

static otError coap_send_toggle(void)
{
	struct openthread_context *ctx = openthread_get_default_context();
	otMessage *msg = NULL;
	otMessageInfo info;
	otError err;

	openthread_api_mutex_lock(ctx);
	otInstance *inst = ctx->instance;

	do {
		if (otThreadGetDeviceRole(inst) < OT_DEVICE_ROLE_CHILD) {
			LOG_WRN("Thread 네트워크에 아직 붙지 않음 - 전송 생략 (ot state 확인)");
			err = OT_ERROR_INVALID_STATE;
			break;
		}

		msg = otCoapNewMessage(inst, NULL);
		if (msg == NULL) {
			err = OT_ERROR_NO_BUFS;
			break;
		}

		otCoapMessageInit(msg, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_PUT);
		otCoapMessageGenerateToken(msg, OT_COAP_DEFAULT_TOKEN_LENGTH);

		err = otCoapMessageAppendUriPathOptions(msg, CONFIG_LIGHT_SWITCH_COAP_URI);
		if (err != OT_ERROR_NONE) {
			break;
		}

		err = otCoapMessageSetPayloadMarker(msg);
		if (err != OT_ERROR_NONE) {
			break;
		}

		err = otMessageAppend(msg, "toggle", strlen("toggle"));
		if (err != OT_ERROR_NONE) {
			break;
		}

		memset(&info, 0, sizeof(info));
		info.mPeerPort = OT_DEFAULT_COAP_PORT;
		err = otIp6AddressFromString(CONFIG_LIGHT_SWITCH_PEER_ADDR, &info.mPeerAddr);
		if (err != OT_ERROR_NONE) {
			break;
		}

		err = otCoapSendRequest(inst, msg, &info, NULL, NULL);
	} while (0);

	if (err != OT_ERROR_NONE && msg != NULL) {
		otMessageFree(msg);
	}

	openthread_api_mutex_unlock(ctx);
	return err;
}

static void send_toggle_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	gpio_pin_set_dt(&led, 1);
	k_work_schedule(&led_off_work, K_MSEC(150));

	otError err = coap_send_toggle();

	if (err == OT_ERROR_NONE) {
		LOG_INF("CoAP PUT coap://[%s]/%s \"toggle\" 전송",
			CONFIG_LIGHT_SWITCH_PEER_ADDR, CONFIG_LIGHT_SWITCH_COAP_URI);
	} else {
		LOG_ERR("CoAP 전송 실패: %d", err);
	}
}

static void button_pressed(const struct device *dev, struct gpio_callback *cb,
			   uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	int64_t now = k_uptime_get();

	if (now - last_press_ms < CONFIG_LIGHT_SWITCH_DEBOUNCE_MS) {
		return;
	}
	last_press_ms = now;

	k_work_submit(&send_work);
}

static int openthread_init(void)
{
	struct openthread_context *ctx = openthread_get_default_context();
	otError err;

	openthread_api_mutex_lock(ctx);
	otInstance *inst = ctx->instance;

	err = otCoapStart(inst, OT_DEFAULT_COAP_PORT);
	if (err != OT_ERROR_NONE) {
		openthread_api_mutex_unlock(ctx);
		LOG_ERR("otCoapStart 실패: %d", err);
		return -EIO;
	}

	/* 저장된 dataset이 있으면(이전에 조인한 적 있으면) 자동 재시작 */
	if (otDatasetIsCommissioned(inst)) {
		otIp6SetEnabled(inst, true);
		otThreadSetEnabled(inst, true);
		LOG_INF("저장된 Thread dataset 발견 - 네트워크 재시작");
	} else {
		LOG_INF("Thread dataset 없음. 셸에서 조인하세요:");
		LOG_INF("  ot dataset set active <OTBR의 dataset hex>");
		LOG_INF("  ot ifconfig up && ot thread start");
	}

	openthread_api_mutex_unlock(ctx);
	return 0;
}

int main(void)
{
	int ret;

	LOG_INF("=== Thing Plus Matter CoAP Light Switch ===");

	if (!gpio_is_ready_dt(&button) || !gpio_is_ready_dt(&led)) {
		LOG_ERR("GPIO 디바이스가 준비되지 않음");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("버튼 핀 설정 실패: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("버튼 인터럽트 설정 실패: %d", ret);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	ret = openthread_init();
	if (ret != 0) {
		return ret;
	}

	LOG_INF("버튼(PB00)을 누르면 CoAP toggle을 전송합니다");
	return 0;
}
