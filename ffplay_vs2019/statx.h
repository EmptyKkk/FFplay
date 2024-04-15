#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <cmath>
#include <thread>
extern "C"
{
	#include "config.h"
	#include "compat/va_copy.h"
	#include "libavformat/avformat.h"
	#include "libavfilter/avfilter.h"
	#include "libavdevice/avdevice.h"
	#include "libavresample/avresample.h"
	#include "libswscale/swscale.h"
	#include "libswresample/swresample.h"
	#include "libpostproc/postprocess.h"
	#include "libavutil/attributes.h"
	#include "libavutil/avassert.h"
	#include "libavutil/avstring.h"
	#include "libavutil/bprint.h"
	#include "libavutil/display.h"
	#include "libavutil/mathematics.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/libm.h"
	#include "libavutil/parseutils.h"
	#include "libavutil/pixdesc.h"
	#include "libavutil/eval.h"
	#include "libavutil/dict.h"
	#include "libavutil/opt.h"
	#include "libavutil/cpu.h"
	//#include "libavutil/ffversion.h"
	#include "libavutil/version.h"
	#include "cmdutils.h"

	#include <SDL.h>
	#include <SDL_thread.h>

	#if CONFIG_NETWORK
	#include "libavformat/network.h"
	#endif

	#if HAVE_SYS_RESOURCE_H
	#include <sys/time.h>
	#include <sys/resource.h>
	#endif


	#ifdef _WIN32
	#include <windows.h>
	#endif
}
