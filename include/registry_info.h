#ifndef __REGISTRY_INFO_H__
#define __REGISTRY_INFO_H__

typedef struct
{
	// /CONFIG/SYSTEM/XMB
	int language;
	int button_assign;

	// /CONFIG/SYSTEM/CHARACTER_SET
	int oem;
	int ansi;

	// /CONFIG/DATE
	int time_format;
	int date_format;
	int summer_time;
	int time_zone_offset;

	// /CONFIG/NETWORK/ADHOC
	int channel;

	// /CONFIG/NETWORK/INFRASTRUCTURE
	int eap_md5;
	int auto_setting;
	int wifisvc_setting;

	// /CONFIG/NP
	int nav_only;
	int np_ad_clock_diff;
	int np_geo_filtering;
	int guest_yob;
	int guest_mob;
	int guest_dob;

	// /CONFIG/SYSTEM/LOCK
	int parental_level;
	int browser_start;

	// /CONFIG/SYSTEM
	char owner_name[128]; //0x50
	
	// /CONFIG/DATE
	char time_zone_area[48]; //0xD0

	char reserved[16];

	// /CONFIG/NP
	char env[9]; //0x110
	char account_id[16]; //0x119
	char login_id[65]; //0x129
	char password[31]; //0x16A
	char guest_country[3]; //0x189
	char guest_lang[3]; //0x18C

	// /CONFIG/SYSTEM/LOCK/password
	char lock_password[4]; //0x18F

	// /CONFIG/NETWORK/ADHOC
	char ssid_prefix[4]; //0x193
} RegistryInfo;

#endif