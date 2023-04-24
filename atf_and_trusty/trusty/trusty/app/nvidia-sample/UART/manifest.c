#include <trusty_app_manifest.h>
#include <stdio.h>
#include <stddef.h>

trusty_app_manifest_t TRUSTY_APP_MANIFEST_ATTRS trusty_app_manifest =
{
	{ 0x1c875825, 0x26a1, 0x40a2,
		{ 0xab, 0x2a, 0xc7, 0xb2, 0xc8, 0xe6, 0x92, 0x80} },

	{
		TRUSTY_APP_CONFIG_MIN_HEAP_SIZE(MIN_HEAP_SIZE),
		TRUSTY_APP_CONFIG_MIN_STACK_SIZE(MIN_STACK_SIZE),
	},	
};
