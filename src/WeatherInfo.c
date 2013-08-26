#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "stdlib.h"
#include "string.h"
#include "config.h"
//#include "my_math.h"
#include "http.h"
#include "util.h"
#include "link_monitor.h"


#define MY_UUID { 0x1A, 0x6B, 0x6D, 0x23, 0x9F, 0x69, 0x41, 0x06, 0x89, 0x5E, 0x11, 0x84, 0xF7, 0xEC, 0xFF, 0x69 }
PBL_APP_INFO(MY_UUID,
             "WeatherInfo", "angelus201",
             1, 0, /* App version */
	     RESOURCE_ID_IMAGE_MENU_ICON,
	     APP_INFO_WATCH_FACE);

Window window;

TextLayer text_temperature_layer;
TextLayer DayOfWeekLayer;
BmpContainer background_image;
BmpContainer time_format_image;
TextLayer mail_layer;				/* layer for mail info */
TextLayer calls_layer;   			/* layer for Phone Calls info */
TextLayer sms_layer;   				/* layer for SMS info */
TextLayer bat_layer;				/* layer for battery info */
TextLayer debug_layer;   			/* layer for DEBUG info */

static int our_latitude, our_longitude, our_timezone = 99;
static bool located = false;
static bool temperature_set = false;

GFont font_temperature;        			/* font for Temperature */

const int DATENUM_IMAGE_RESOURCE_IDS[] = {
	RESOURCE_ID_IMAGE_DATENUM_0,
	RESOURCE_ID_IMAGE_DATENUM_1,
	RESOURCE_ID_IMAGE_DATENUM_2,
	RESOURCE_ID_IMAGE_DATENUM_3,
	RESOURCE_ID_IMAGE_DATENUM_4,
	RESOURCE_ID_IMAGE_DATENUM_5,
	RESOURCE_ID_IMAGE_DATENUM_6,
	RESOURCE_ID_IMAGE_DATENUM_7,
	RESOURCE_ID_IMAGE_DATENUM_8,
	RESOURCE_ID_IMAGE_DATENUM_9
};


#define TOTAL_WEATHER_IMAGES 1
BmpContainer weather_images[TOTAL_WEATHER_IMAGES];

const int WEATHER_IMAGE_RESOURCE_IDS[] = {
	RESOURCE_ID_IMAGE_CLEAR_DAY,
	RESOURCE_ID_IMAGE_CLEAR_NIGHT,
	RESOURCE_ID_IMAGE_RAIN,
	RESOURCE_ID_IMAGE_SNOW,
	RESOURCE_ID_IMAGE_SLEET,
	RESOURCE_ID_IMAGE_WIND,
	RESOURCE_ID_IMAGE_FOG,
	RESOURCE_ID_IMAGE_CLOUDY,
	RESOURCE_ID_IMAGE_PARTLY_CLOUDY_DAY,
	RESOURCE_ID_IMAGE_PARTLY_CLOUDY_NIGHT,
	RESOURCE_ID_IMAGE_THUNDER,
	RESOURCE_ID_IMAGE_RAIN_SNOW,
	RESOURCE_ID_IMAGE_SNOW_SLEET,
	RESOURCE_ID_IMAGE_COLD,
	RESOURCE_ID_IMAGE_HOT,
	RESOURCE_ID_IMAGE_NO_WEATHER
};

#define TOTAL_DATE_DIGITS 8
BmpContainer date_digits_images[TOTAL_DATE_DIGITS];

const int BIG_DIGIT_IMAGE_RESOURCE_IDS[] = {
	RESOURCE_ID_IMAGE_NUM_0,
	RESOURCE_ID_IMAGE_NUM_1,
	RESOURCE_ID_IMAGE_NUM_2,
	RESOURCE_ID_IMAGE_NUM_3,
	RESOURCE_ID_IMAGE_NUM_4,
	RESOURCE_ID_IMAGE_NUM_5,
	RESOURCE_ID_IMAGE_NUM_6,
	RESOURCE_ID_IMAGE_NUM_7,
	RESOURCE_ID_IMAGE_NUM_8,
	RESOURCE_ID_IMAGE_NUM_9
};

#define TOTAL_TIME_DIGITS 4
BmpContainer time_digits_images[TOTAL_TIME_DIGITS];

void set_container_image(BmpContainer *bmp_container, const int resource_id, GPoint origin) {

	layer_remove_from_parent(&bmp_container->layer.layer);
	bmp_deinit_container(bmp_container);

	bmp_init_container(resource_id, bmp_container);

	GRect frame = layer_get_frame(&bmp_container->layer.layer);
	frame.origin.x = origin.x;
	frame.origin.y = origin.y;
	layer_set_frame(&bmp_container->layer.layer, frame);

	layer_add_child(&window.layer, &bmp_container->layer.layer);
}

unsigned short get_display_hour(unsigned short hour) {
  
	if (clock_is_24h_style()) {
		return hour;
	}

	unsigned short display_hour = hour % 12;

	// Converts "0" to "12"
	return display_hour ? display_hour : 12;
}

void adjustTimezone(float* time) {

	*time += our_timezone;
	if (*time > 24) *time -= 24;
	if (*time < 0) *time += 24;
}

unsigned short the_last_hour = 25;

void request_weather();

void display_counters(TextLayer *dataLayer, struct Data d, int infoType) {
	
	static char temp_text[5];
	
	if (d.link_status != LinkStatusOK) {
		memcpy(temp_text, "?", 1);
	}
	else {	
		if (infoType == 1) {
			if (d.missed) {
				memcpy(temp_text, itoa(d.missed), 4);
			}
			else {
				memcpy(temp_text, itoa(0), 4);
			}
		}
		else if (infoType == 2) {
			if (d.unread) {
				memcpy(temp_text, itoa(d.unread), 4);
			}
			else {
				memcpy(temp_text, itoa(0), 4);
			}
		}
	}
	
	text_layer_set_text(dataLayer, temp_text);
}

void failed(int32_t cookie, int http_status, void* context) {
	
	if ((cookie == 0 || cookie == WEATHER_HTTP_COOKIE) && !temperature_set) {
		set_container_image(&weather_images[0], WEATHER_IMAGE_RESOURCE_IDS[10], GPoint(22, 3));
		text_layer_set_text(&text_temperature_layer, "---°");
	}
	
	//link_monitor_handle_failure(http_status);
	
	//Re-request the location and subsequently weather on next minute tick
	//located = false;
}

void success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
	
	if (cookie != WEATHER_HTTP_COOKIE) return;
	
	Tuple* icon_tuple = dict_find(received, WEATHER_KEY_ICON);
	if (icon_tuple) {
		int icon = icon_tuple->value->int8;
		if (icon >= 0 && icon < 16) {
			set_container_image(&weather_images[0], WEATHER_IMAGE_RESOURCE_IDS[icon], GPoint(22, 3));  // ---------- Weather Image
		} else {
			set_container_image(&weather_images[0], WEATHER_IMAGE_RESOURCE_IDS[15], GPoint(22, 3));
		}
	}
	
	Tuple* temperature_tuple = dict_find(received, WEATHER_KEY_TEMPERATURE);
	if (temperature_tuple) {
		
		static char temp_text[5];
		memcpy(temp_text, itoa(temperature_tuple->value->int16), 4);
		int degree_pos = strlen(temp_text);
		memcpy(&temp_text[degree_pos], "°", 3);
		text_layer_set_text(&text_temperature_layer, temp_text);
		temperature_set = true;
	}
	
	link_monitor_handle_success(&data);
	//display_counters(&calls_layer, data, 1);
	//display_counters(&sms_layer, data, 2);
}

void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
	
	// Fix the floats
	our_latitude = latitude * 1000000;
	our_longitude = longitude * 1000000;
	located = true;
	request_weather();
}

void reconnect(void* context) {
	
	located = false;
	request_weather();
}

bool read_state_data(DictionaryIterator* received, struct Data* d) {
	
	(void)d;
	bool has_data = false;
	Tuple* tuple = dict_read_first(received);
	if (!tuple) return false;
	do {
		switch(tuple->key) {
	  		case TUPLE_MISSED_CALLS:
				d->missed = tuple->value->uint8;
				
				static char temp_calls[5];
				memcpy(temp_calls, itoa(tuple->value->uint8), 4);
				text_layer_set_text(&calls_layer, temp_calls);
				
				has_data = true;
				break;
			case TUPLE_UNREAD_SMS:
				d->unread = tuple->value->uint8;
			
				static char temp_sms[5];
				memcpy(temp_sms, itoa(tuple->value->uint8), 4);
				text_layer_set_text(&sms_layer, temp_sms);
			
				has_data = true;
				break;
		}
	}
	while((tuple = dict_read_next(received)));
	return has_data;
}

void app_received_msg(DictionaryIterator* received, void* context) {

	link_monitor_handle_success(&data);
	if (read_state_data(received, &data)) 
	{
		//display_counters(&calls_layer, data, 1);
		//display_counters(&sms_layer, data, 2);
		if (!located)
		{
			request_weather();
		}
	}
}
static void app_send_failed(DictionaryIterator* failed, AppMessageResult reason, void* context) {

	link_monitor_handle_failure(reason, &data);
	//display_counters(&calls_layer, data, 1);
	//display_counters(&sms_layer, data, 2);
}

bool register_callbacks() {

	if (callbacks_registered) {
		if (app_message_deregister_callbacks(&app_callbacks) == APP_MSG_OK)
			callbacks_registered = false;
	}
	if (!callbacks_registered) {
		app_callbacks = (AppMessageCallbacksNode) {
			.callbacks = { .in_received = app_received_msg, .out_failed = app_send_failed} };
		if (app_message_register_callbacks(&app_callbacks) == APP_MSG_OK) {
      callbacks_registered = true;
      }
	}
	return callbacks_registered;
}


void receivedtime(int32_t utc_offset_seconds, bool is_dst, uint32_t unixtime, const char* tz_name, void* context) {

	our_timezone = (utc_offset_seconds / 3600);
	if (is_dst) {
		our_timezone--;
	}
}

void update_display(PblTm *current_time) {
  
	unsigned short display_hour = get_display_hour(current_time->tm_hour);
  
	//Hour
	set_container_image(&time_digits_images[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour/10], GPoint(4, 75));
	set_container_image(&time_digits_images[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour%10], GPoint(37, 75));
  
	//Minute
	set_container_image(&time_digits_images[2], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min/10], GPoint(80, 75));
	set_container_image(&time_digits_images[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min%10], GPoint(111, 75));
   
	if (the_last_hour != display_hour) {
		// Day of week
		text_layer_set_text(&DayOfWeekLayer, DAY_NAME_LANGUAGE[current_time->tm_wday]); 
	  
		// Day
		set_container_image(&date_digits_images[0], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday/10], GPoint(day_month_x[0], 52));
		set_container_image(&date_digits_images[1], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday%10], GPoint(day_month_x[0] + 13, 52));
	  
		// Month
		set_container_image(&date_digits_images[2], DATENUM_IMAGE_RESOURCE_IDS[(current_time->tm_mon+1)/10], GPoint(day_month_x[1], 52));
		set_container_image(&date_digits_images[3], DATENUM_IMAGE_RESOURCE_IDS[(current_time->tm_mon+1)%10], GPoint(day_month_x[1] + 13, 52));
	  
		// Year
		set_container_image(&date_digits_images[4], DATENUM_IMAGE_RESOURCE_IDS[((1900+current_time->tm_year)%1000)/10], GPoint(day_month_x[2], 52));
		set_container_image(&date_digits_images[5], DATENUM_IMAGE_RESOURCE_IDS[((1900+current_time->tm_year)%1000)%10], GPoint(day_month_x[2] + 13, 52));
		
		if (!clock_is_24h_style()) {
			if (display_hour/10 == 0) {
				layer_remove_from_parent(&time_digits_images[0].layer.layer);
				bmp_deinit_container(&time_digits_images[0]);
			}
		}
	    
	  	the_last_hour = display_hour;
  	}
	
}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {

	(void)ctx;	
	update_display(t->tick_time);
	
	if (!located || !(t->tick_time->tm_min % 15)) {
		// Every 15 minutes, request updated weather
		http_location_request();
	}
	
	// Every 15 minutes, request updated time
	http_time_request();
		
	if (!(t->tick_time->tm_min % 2) || data.link_status == LinkStatusUnknown) link_monitor_ping();
}

void handle_init(AppContextRef ctx) {

	(void)ctx;
	window_init(&window, "WeatherInfo");
	window_stack_push(&window, true /* Animated */);
	window_set_background_color(&window, GColorBlack);
	resource_init_current_app(&APP_RESOURCES);
	
	// Load Fonts
	font_temperature = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_32));

	bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image);
	layer_add_child(&window.layer, &background_image.layer.layer);
  
	// Text for Temperature
	text_layer_init(&text_temperature_layer, window.layer.frame);
	text_layer_set_text_color(&text_temperature_layer, GColorWhite);
	text_layer_set_background_color(&text_temperature_layer, GColorClear);
	layer_set_frame(&text_temperature_layer.layer, GRect(78, 3, 64, 68));
	text_layer_set_font(&text_temperature_layer, font_temperature);
	layer_add_child(&window.layer, &text_temperature_layer.layer);  
  
	// Day of week text
	text_layer_init(&DayOfWeekLayer, GRect(5, 43, 130 /* width */, 30 /* height */));
	layer_add_child(&background_image.layer.layer, &DayOfWeekLayer.layer);
	text_layer_set_text_color(&DayOfWeekLayer, GColorWhite);
	text_layer_set_background_color(&DayOfWeekLayer, GColorClear);
	text_layer_set_font(&DayOfWeekLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

	// Mail Info layer
	text_layer_init(&mail_layer, GRect(10, 152, 27, 14));
	text_layer_set_text_color(&mail_layer, GColorWhite);
	text_layer_set_text_alignment(&mail_layer, GTextAlignmentCenter);
	text_layer_set_background_color(&mail_layer, GColorClear);
	text_layer_set_font(&mail_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(&window.layer, &mail_layer.layer);
	text_layer_set_text(&mail_layer, "-");

	// SMS Info layer 
	text_layer_init(&sms_layer, GRect(45, 152, 30, 14));
	text_layer_set_text_color(&sms_layer, GColorWhite);
	text_layer_set_text_alignment(&sms_layer, GTextAlignmentCenter);
	text_layer_set_background_color(&sms_layer, GColorClear);
	text_layer_set_font(&sms_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(&window.layer, &sms_layer.layer);
	text_layer_set_text(&sms_layer, "-");

	// Calls Info layer
	text_layer_init(&calls_layer, GRect(84, 152, 26, 14));
	text_layer_set_text_color(&calls_layer, GColorWhite);
	text_layer_set_text_alignment(&calls_layer, GTextAlignmentCenter);
	text_layer_set_background_color(&calls_layer, GColorClear);
	text_layer_set_font(&calls_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(&window.layer, &calls_layer.layer);
	text_layer_set_text(&calls_layer, "-");

	// Battery Info layer
	text_layer_init(&bat_layer, GRect(112, 152, 26, 14));
	text_layer_set_text_color(&bat_layer, GColorWhite);
	text_layer_set_text_alignment(&bat_layer, GTextAlignmentCenter);
	text_layer_set_background_color(&bat_layer, GColorClear);
	text_layer_set_font(&bat_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(&window.layer, &bat_layer.layer);
	text_layer_set_text(&bat_layer, "-");

	// DEBUG Info layer 
	text_layer_init(&debug_layer, window.layer.frame);
	text_layer_set_text_color(&debug_layer, GColorWhite);
	text_layer_set_background_color(&debug_layer, GColorClear);
	layer_set_frame(&debug_layer.layer, GRect(50, 152, 100, 30));
	text_layer_set_font(&debug_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(&window.layer, &debug_layer.layer);
	
	data.link_status = LinkStatusUnknown;
	link_monitor_ping();
	
	// request data refresh on window appear (for example after notification)
	WindowHandlers handlers = { .appear = &link_monitor_ping};
	window_set_window_handlers(&window, handlers);
	
	http_register_callbacks((HTTPCallbacks) {.failure=failed,.success=success,.reconnect=reconnect,.location=location,.time=receivedtime}, (void*)ctx);
	register_callbacks();
	
	// Avoids a blank screen on watch start.
	PblTm tick_time;

	get_time(&tick_time);
	update_display(&tick_time);

}


void handle_deinit(AppContextRef ctx) {
	
	(void)ctx;
	bmp_deinit_container(&background_image);
	bmp_deinit_container(&time_format_image);

	for (int i = 0; i < TOTAL_DATE_DIGITS; i++) {
		bmp_deinit_container(&date_digits_images[i]);
	}
	
	for (int i = 0; i < TOTAL_TIME_DIGITS; i++) {
		bmp_deinit_container(&time_digits_images[i]);
	}
	
	fonts_unload_custom_font(font_temperature);

}

void pbl_main(void *params) {

	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.deinit_handler = &handle_deinit,
		.tick_info = {
			.tick_handler = &handle_minute_tick,
			.tick_units = MINUTE_UNIT
		},
		.messaging_info = {
			.buffer_sizes = {
				.inbound = 124,
				.outbound = 256,
			}
		}
	};

	app_event_loop(params, &handlers);
}

void request_weather() {
	
	if (!located) {
		http_location_request();
		return;
	}
	
	// Build the HTTP request
	DictionaryIterator *body;
	//HTTPResult result = http_out_get("https://ofkorth.net/pebble/weather.php", WEATHER_HTTP_COOKIE, &body);
	//HTTPResult result = http_out_get("http://www.zone-mr.net/api/weather.php", WEATHER_HTTP_COOKIE, &body);
	HTTPResult result = http_out_get("http://www.mirz.com/httpebble/weather-yahoo.php", WEATHER_HTTP_COOKIE, &body);
	if (result != HTTP_OK) {
		return;
	}
	
	dict_write_int32(body, WEATHER_KEY_LATITUDE, our_latitude);
	dict_write_int32(body, WEATHER_KEY_LONGITUDE, our_longitude);
	dict_write_cstring(body, WEATHER_KEY_UNIT_SYSTEM, UNIT_SYSTEM);
	
	// Send it.
	if (http_out_send() != HTTP_OK) {
		return;
	}
	
	// Request updated Time
	http_time_request();
}
