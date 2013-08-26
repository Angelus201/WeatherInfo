#ifndef CONFIG_H
#define CONFIG_H

//NOTE: longitude is positive for East and negative for West
#define LATITUDE    				51.0
#define LONGITUDE 				8.0
#define TIMEZONE 				0
#define DAY_NAME_LANGUAGE 			DAY_NAME_GERMAN 		// Valid values: DAY_NAME_GERMAN
#define day_month_x 				day_month_day_first 		// Valid values: day_month_month_first, day_month_day_first

// Any of "us", "ca", "uk" (for idiosyncratic US, Candian and British measurements),
// "si" (for pure metric) or "auto" (determined by the above latitude/longitude)
	
// For the Yahoo Weather only C or F works
#define UNIT_SYSTEM "c"

// POST variables
#define WEATHER_KEY_LATITUDE 1
#define WEATHER_KEY_LONGITUDE 2
#define WEATHER_KEY_UNIT_SYSTEM 3
	
// Received variables
#define WEATHER_KEY_ICON 1
#define WEATHER_KEY_TEMPERATURE 2

#define WEATHER_HTTP_COOKIE 1949327671
#define TIME_HTTP_COOKIE 1131038282

// ---- Constants for all available languages ----------------------------------------

const int day_month_day_first[] = {
	55,
	87,
	115
};

const int day_month_month_first[] = {
	87,
	55,
	115
};

const char *DAY_NAME_GERMAN[] = {
	"SO",
	"MO",
	"DI",
	"MI",
	"DO",
	"FR",
	"SA"
};

#endif