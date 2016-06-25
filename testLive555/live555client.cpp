/******************************************************************************************************
 * class Live555Client using live555 library to create a client for RTSP, base from code of VLC       *
 * www.videolan.org                                                                                   *
 * Author: HungNV (hung.viet.nguyen.hp at gmail dot com)                                              *
 * Date  : 2016-05-17 - 15:00                                                                         *
 ******************************************************************************************************
 * This program is free software; you can redistribute it and/or modify it                            *
 * under the terms of the GNU Lesser General Public License as published by                           *
 * the Free Software Foundation; either version 2.1 of the License, or                                *
 * (at your option) any later version.                                                                *
 *                                                                                                    *
 * This program is distributed in the hope that it will be useful,                                    *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                                     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                       *
 * GNU Lesser General Public License for more details.                                                *
 *                                                                                                    *
 * You should have received a copy of the GNU Lesser General Public License                           *
 * along with this program; if not, write to the Free Software Foundation,                            *
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.                                  *
 ******************************************************************************************************/

#include "live555client.h"
#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include <liveMedia_version.hh>
#include <Base64.hh>
#include <chrono>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#include <Windows.h>
#pragma warning(disable:4996)
#endif

#if defined(_WIN32) || defined(WIN32)
char *strcasestr(const char *phaystack, const char *pneedle)
	// To make this work with MS Visual C++, this version uses tolower/toupper() in place of
	// _tolower/_toupper(), since apparently in GNU C, the underscore macros are identical
	// to the non-underscore versions; but in MS the underscore ones do an unconditional
	// conversion (mangling non-alphabetic characters such as the zero terminator).  MSDN:
	// tolower: Converts c to lowercase if appropriate
	// _tolower: Converts c to lowercase

	// Return the offset of one string within another.
	// Copyright (C) 1994,1996,1997,1998,1999,2000 Free Software Foundation, Inc.
	// This file is part of the GNU C Library.

	// The GNU C Library is free software; you can redistribute it and/or
	// modify it under the terms of the GNU Lesser General Public
	// License as published by the Free Software Foundation; either
	// version 2.1 of the License, or (at your option) any later version.

	// The GNU C Library is distributed in the hope that it will be useful,
	// but WITHOUT ANY WARRANTY; without even the implied warranty of
	// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	// Lesser General Public License for more details.

	// You should have received a copy of the GNU Lesser General Public
	// License along with the GNU C Library; if not, write to the Free
	// Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
	// 02111-1307 USA.

	// My personal strstr() implementation that beats most other algorithms.
	// Until someone tells me otherwise, I assume that this is the
	// fastest implementation of strstr() in C.
	// I deliberately chose not to comment it.  You should have at least
	// as much fun trying to understand it, as I had to write it :-).
	// Stephen R. van den Berg, berg@pool.informatik.rwth-aachen.de

	// Faster looping by precalculating bl, bu, cl, cu before looping.
	// 2004 Apr 08	Jose Da Silva, digital@joescat@com
{
	register const unsigned char *haystack, *needle;
	register unsigned bl, bu, cl, cu;
	
	haystack = (const unsigned char *) phaystack;
	needle = (const unsigned char *) pneedle;

	bl = tolower(*needle);
	if (bl != '\0')
	{
		// Scan haystack until the first character of needle is found:
		bu = toupper(bl);
		haystack--;				/* possible ANSI violation */
		do
		{
			cl = *++haystack;
			if (cl == '\0')
				goto ret0;
		}
		while ((cl != bl) && (cl != bu));

		// See if the rest of needle is a one-for-one match with this part of haystack:
		cl = tolower(*++needle);
		if (cl == '\0')  // Since needle consists of only one character, it is already a match as found above.
			goto foundneedle;
		cu = toupper(cl);
		++needle;
		goto jin;
		
		for (;;)
		{
			register unsigned a;
			register const unsigned char *rhaystack, *rneedle;
			do
			{
				a = *++haystack;
				if (a == '\0')
					goto ret0;
				if ((a == bl) || (a == bu))
					break;
				a = *++haystack;
				if (a == '\0')
					goto ret0;
shloop:
				;
			}
			while ((a != bl) && (a != bu));

jin:
			a = *++haystack;
			if (a == '\0')  // Remaining part of haystack is shorter than needle.  No match.
				goto ret0;

			if ((a != cl) && (a != cu)) // This promising candidate is not a complete match.
				goto shloop;            // Start looking for another match on the first char of needle.
			
			rhaystack = haystack-- + 1;
			rneedle = needle;
			a = tolower(*rneedle);
			
			if (tolower(*rhaystack) == (int) a)
			do
			{
				if (a == '\0')
					goto foundneedle;
				++rhaystack;
				a = tolower(*++needle);
				if (tolower(*rhaystack) != (int) a)
					break;
				if (a == '\0')
					goto foundneedle;
				++rhaystack;
				a = tolower(*++needle);
			}
			while (tolower(*rhaystack) == (int) a);
			
			needle = rneedle;		/* took the register-poor approach */
			
			if (a == '\0')
				break;
		} // for(;;)
	} // if (bl != '\0')
foundneedle:
	return (char*) haystack;
ret0:
	return 0;
}
#endif

/* All timestamp below or equal to this define are invalid/unset
 * XXX the numerical value is 0 because of historical reason and will change.*/
#define VLC_TS_INVALID INT64_C(0)
#define VLC_TS_0 INT64_C(1)

#define CLOCK_FREQ INT64_C(1000000)

/*****************************************************************************
 * Interface configuration
 *****************************************************************************/

/* Base delay in micro second for interface sleeps */
#define INTF_IDLE_SLEEP                 (CLOCK_FREQ/20)

/*****************************************************************************
 * Input thread configuration
 *****************************************************************************/

/* Used in ErrorThread */
#define INPUT_IDLE_SLEEP                (CLOCK_FREQ/10)

/*
 * General limitations
 */

/* Duration between the time we receive the data packet, and the time we will
 * mark it to be presented */
#define DEFAULT_PTS_DELAY               (3*CLOCK_FREQ/10)

/*****************************************************************************
 * SPU configuration
 *****************************************************************************/

/* Buffer must avoid arriving more than SPU_MAX_PREPARE_TIME in advanced to
 * the SPU */
#define SPU_MAX_PREPARE_TIME            (CLOCK_FREQ/2)

static inline Boolean toBool( bool b ) { return b?True:False; } // silly, no?

static /* Base64 decoding */
size_t vlc_b64_decode_binary_to_buffer( uint8_t *p_dst, size_t i_dst, const char *p_src )
{
    static const int b64[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };
    uint8_t *p_start = p_dst;
    uint8_t *p = (uint8_t *)p_src;

    int i_level;
    int i_last;

    for( i_level = 0, i_last = 0; (size_t)( p_dst - p_start ) < i_dst && *p != '\0'; p++ )
    {
        const int c = b64[(unsigned int)*p];
        if( c == -1 )
            break;

        switch( i_level )
        {
            case 0:
                i_level++;
                break;
            case 1:
                *p_dst++ = ( i_last << 2 ) | ( ( c >> 4)&0x03 );
                i_level++;
                break;
            case 2:
                *p_dst++ = ( ( i_last << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
                i_level++;
                break;
            case 3:
                *p_dst++ = ( ( i_last &0x03 ) << 6 ) | c;
                i_level = 0;
        }
        i_last = c;
    }

    return p_dst - p_start;
}

//static size_t vlc_b64_decode_binary( uint8_t **pp_dst, const char *psz_src )
//{
//    const int i_src = strlen( psz_src );
//    uint8_t   *p_dst;
//
//    *pp_dst = p_dst = malloc( i_src );
//    if( !p_dst )
//        return 0;
//    return  vlc_b64_decode_binary_to_buffer( p_dst, i_src, psz_src );
//}
//
//static char *vlc_b64_decode( const char *psz_src )
//{
//    const int i_src = strlen( psz_src );
//    char *p_dst = malloc( i_src + 1 );
//    size_t i_dst;
//    if( !p_dst )
//        return NULL;
//
//    i_dst = vlc_b64_decode_binary_to_buffer( (uint8_t*)p_dst, i_src, psz_src );
//    p_dst[i_dst] = '\0';
//
//    return p_dst;
//}

//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRTSPClient : public RTSPClient
{
protected:
	Live555Client* parent;
public:
    MyRTSPClient( UsageEnvironment& env, char const* rtspURL, int verbosityLevel,
                   char const* applicationName, portNumBits tunnelOverHTTPPortNum,
                   Live555Client *p_sys) 
		: RTSPClient( env, rtspURL, verbosityLevel, applicationName,
                   tunnelOverHTTPPortNum
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1373932800
                   , -1
#endif
                   )
				   , parent (p_sys)
    {
    }

	static void continueAfterDESCRIBE(RTSPClient* client, int result_code, char* result_string );
	static void continueAfterOPTIONS( RTSPClient* client, int result_code, char* result_string );
	static void default_live555_callback( RTSPClient* client, int result_code, char* result_string );
	static unsigned char* parseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize );
	static uint8_t * parseVorbisConfigStr( char const* configStr,
                                      unsigned int& configSize );
};

void MyRTSPClient::continueAfterDESCRIBE(RTSPClient* client, int result_code, char* result_string )
{
	MyRTSPClient* pThis = static_cast<MyRTSPClient*>(client);
	pThis->parent->continueAfterDESCRIBE(result_code, result_string);
	delete[] result_string;
}

void MyRTSPClient::continueAfterOPTIONS( RTSPClient* client, int result_code, char* result_string )
{
	MyRTSPClient* pThis = static_cast<MyRTSPClient*>(client);
	pThis->parent->continueAfterOPTIONS(result_code, result_string);
	delete[] result_string;
}

void MyRTSPClient::default_live555_callback( RTSPClient* client, int result_code, char* result_string )
{
	MyRTSPClient* pThis = static_cast<MyRTSPClient*>(client);
	delete []result_string;
	pThis->parent->live555Callback(result_code);
}

unsigned char* MyRTSPClient::parseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize )
{
    char *dup, *psz;
    size_t i_records = 1;

    configSize = 0;

    if( configStr == NULL || *configStr == '\0' )
        return NULL;

    psz = dup = strdup( configStr );

    /* Count the number of commas */
    for( psz = dup; *psz != '\0'; ++psz )
    {
        if( *psz == ',')
        {
            ++i_records;
            *psz = '\0';
        }
    }

    size_t configMax = 5*strlen(dup);
    unsigned char *cfg = new unsigned char[configMax];
    psz = dup;
    for( size_t i = 0; i < i_records; ++i )
    {
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x01;

        configSize += vlc_b64_decode_binary_to_buffer( cfg+configSize,
                                          configMax-configSize, psz );
        psz += strlen(psz)+1;
    }

    free( dup );
    return cfg;
}

uint8_t * MyRTSPClient::parseVorbisConfigStr( char const* configStr,
                                      unsigned int& configSize )
{
    configSize = 0;
    if( configStr == NULL || *configStr == '\0' )
        return NULL;
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1332115200 // 2012.03.20
    unsigned char *p_cfg = base64Decode( configStr, configSize );
#else
    char* configStr_dup = strdup( configStr );
    unsigned char *p_cfg = base64Decode( configStr_dup, configSize );
    free( configStr_dup );
#endif
    uint8_t *p_extra = NULL;
    /* skip header count, ident number and length (cf. RFC 5215) */
    const unsigned int headerSkip = 9;
    if( configSize > headerSkip && ((uint8_t*)p_cfg)[3] == 1 )
    {
        configSize -= headerSkip;
        p_extra = new uint8_t[ configSize ];
        memcpy( p_extra, p_cfg+headerSkip, configSize );
    }
    delete[] p_cfg;
    return p_extra;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
Live555Client::LiveTrack::LiveTrack(Live555Client* p_sys, void* sub, int buffer_size)
	: parent(p_sys)
	, media_sub_session(sub)
	, i_buffer(buffer_size)
{
	//p_es        = NULL;
	b_quicktime = false;
	b_asf       = false;
	//p_asf_block = NULL;
	b_muxed     = false;
	b_discard_trunc = false;
	//p_out_muxed = NULL;
	waiting     = 0;
	b_rtcp_sync = false;
	i_pts       = VLC_TS_INVALID;
	f_npt       = 0.;
	b_selected  = true;

	memset (&fmt, 0, sizeof(fmt));
}

Live555Client::LiveTrack::~LiveTrack()
{
	if (p_buffer) {
		delete[] p_buffer;
		p_buffer = NULL;
	}

	if (fmt.p_extra) {
		delete[] fmt.p_extra;
		fmt.p_extra = NULL;
	}
}

int Live555Client::LiveTrack::init()
{
	MediaSubsession* sub = static_cast<MediaSubsession*>(media_sub_session);
	p_buffer    = new uint8_t [i_buffer + 4];

	if (!p_buffer)
		return -1;

	/* Value taken from mplayer */
    if( !strcmp( sub->mediumName(), "audio" ) )
    {
        //es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('u','n','d','f') );
		fmt.i_cat = AUDIO_ES;
        fmt.audio.i_channels = sub->numChannels();
        fmt.audio.i_rate = sub->rtpTimestampFrequency();

        if( !strcmp( sub->codecName(), "MPA" ) ||
            !strcmp( sub->codecName(), "MPA-ROBUST" ) ||
            !strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) )
        {
            fmt.i_codec = VLC_CODEC_MPGA;
            fmt.audio.i_rate = 0;
        }
        else if( !strcmp( sub->codecName(), "AC3" ) )
        {
            fmt.i_codec = VLC_CODEC_A52;
            fmt.audio.i_rate = 0;
        }
        else if( !strcmp( sub->codecName(), "L16" ) )
        {
            fmt.i_codec = VLC_CODEC_S16B;
            fmt.audio.i_bitspersample = 16;
        }
        else if( !strcmp( sub->codecName(), "L20" ) )
        {
            fmt.i_codec = VLC_CODEC_S20B;
            fmt.audio.i_bitspersample = 20;
        }
        else if( !strcmp( sub->codecName(), "L24" ) )
        {
            fmt.i_codec = VLC_CODEC_S24B;
            fmt.audio.i_bitspersample = 24;
        }
        else if( !strcmp( sub->codecName(), "L8" ) )
        {
            fmt.i_codec = VLC_CODEC_U8;
            fmt.audio.i_bitspersample = 8;
        }
        else if( !strcmp( sub->codecName(), "DAT12" ) )
        {
            fmt.i_codec = VLC_CODEC_DAT12;
            fmt.audio.i_bitspersample = 12;
        }
        else if( !strcmp( sub->codecName(), "PCMU" ) )
        {
            fmt.i_codec = VLC_CODEC_MULAW;
            fmt.audio.i_bitspersample = 8;
        }
        else if( !strcmp( sub->codecName(), "PCMA" ) )
        {
            fmt.i_codec = VLC_CODEC_ALAW;
            fmt.audio.i_bitspersample = 8;
        }
        else if( !strncmp( sub->codecName(), "G726", 4 ) )
        {
            fmt.i_codec = VLC_CODEC_ADPCM_G726;
            fmt.audio.i_rate = 8000;
            fmt.audio.i_channels = 1;
            if( !strcmp( sub->codecName()+5, "40" ) )
                fmt.i_bitrate = 40000;
            else if( !strcmp( sub->codecName()+5, "32" ) )
                fmt.i_bitrate = 32000;
            else if( !strcmp( sub->codecName()+5, "24" ) )
                fmt.i_bitrate = 24000;
            else if( !strcmp( sub->codecName()+5, "16" ) )
                fmt.i_bitrate = 16000;
        }
        else if( !strcmp( sub->codecName(), "AMR" ) )
        {
            fmt.i_codec = VLC_CODEC_AMR_NB;
        }
        else if( !strcmp( sub->codecName(), "AMR-WB" ) )
        {
            fmt.i_codec = VLC_CODEC_AMR_WB;
        }
        else if( !strcmp( sub->codecName(), "MP4A-LATM" ) )
        {
            unsigned int i_extra;
            uint8_t      *p_extra;

            fmt.i_codec = VLC_CODEC_MP4A;

            if( ( p_extra = parseStreamMuxConfigStr( sub->fmtp_config(),
                                                        i_extra ) ) )
            {
                fmt.i_extra = i_extra;
                fmt.p_extra = new uint8_t[ i_extra ];
                memcpy( fmt.p_extra, p_extra, i_extra );
                delete[] p_extra;
            }
            /* Because the "faad" decoder does not handle the LATM
                * data length field at the start of each returned LATM
                * frame, tell the RTP source to omit. */
            ((MPEG4LATMAudioRTPSource*)sub->rtpSource())->omitLATMDataLengthField();
        }
        else if( !strcmp( sub->codecName(), "MPEG4-GENERIC" ) )
        {
            unsigned int i_extra;
            uint8_t      *p_extra;

            fmt.i_codec = VLC_CODEC_MP4A;

            if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                    i_extra ) ) )
            {
                fmt.i_extra = i_extra;
                fmt.p_extra = new uint8_t[ i_extra ];
                memcpy( fmt.p_extra, p_extra, i_extra );
                delete[] p_extra;
            }
        }
        else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
        {
            b_asf = true;
			// handle later
            //if( p_sys->p_out_asf == NULL )
            //    p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
            //                                        p_demux->out );
        }
        else if( !strcmp( sub->codecName(), "X-QT" ) ||
                    !strcmp( sub->codecName(), "X-QUICKTIME" ) )
        {
            b_quicktime = true;
        }
        else if( !strcmp( sub->codecName(), "SPEEX" ) )
        {
            fmt.i_codec = VLC_FOURCC( 's', 'p', 'x', 'r' );
            if ( fmt.audio.i_rate == 0 )
            {
                //msg_Warn( p_demux,"Using 8kHz as default sample rate." );
                fmt.audio.i_rate = 8000;
            }
        }
        else if( !strcmp( sub->codecName(), "VORBIS" ) )
        {
            fmt.i_codec = VLC_CODEC_VORBIS;
            unsigned int i_extra;
            unsigned char *p_extra;
            if( ( p_extra = MyRTSPClient::parseVorbisConfigStr( sub->fmtp_config(),
                                                i_extra ) ) )
            {
                fmt.i_extra = i_extra;
                fmt.p_extra = p_extra;
            }
            //else
            //    msg_Warn( p_demux,"Missing or unsupported vorbis header." );
        }
    }
    else if( !strcmp( sub->mediumName(), "video" ) )
    {
        //es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('u','n','d','f') );
		fmt.i_cat = VIDEO_ES;
        if( !strcmp( sub->codecName(), "MPV" ) )
        {
            fmt.i_codec = VLC_CODEC_MPGV;
            fmt.b_packetized = false;
        }
        else if( !strcmp( sub->codecName(), "H263" ) ||
                    !strcmp( sub->codecName(), "H263-1998" ) ||
                    !strcmp( sub->codecName(), "H263-2000" ) )
        {
            fmt.i_codec = VLC_CODEC_H263;
        }
        else if( !strcmp( sub->codecName(), "H261" ) )
        {
            fmt.i_codec = VLC_CODEC_H261;
        }
        else if( !strcmp( sub->codecName(), "H264" ) )
        {
            unsigned int i_extra = 0;
            uint8_t      *p_extra = NULL;

            fmt.i_codec = VLC_CODEC_H264;
            fmt.b_packetized = false;

            if((p_extra = MyRTSPClient::parseH264ConfigStr( sub->fmtp_spropparametersets(),
                                            i_extra ) ) )
            {
                fmt.i_extra = i_extra;
                fmt.p_extra = new uint8_t[ i_extra ];
                memcpy( fmt.p_extra, p_extra, i_extra );

                delete[] p_extra;
            }
        }
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1393372800 // 2014.02.26
        else if( !strcmp( sub->codecName(), "H265" ) )
        {
            unsigned int i_extra1 = 0, i_extra2 = 0, i_extra3 = 0, i_extraTot;
            uint8_t      *p_extra1 = NULL, *p_extra2 = NULL, *p_extra3 = NULL;

            fmt.i_codec = VLC_CODEC_HEVC;
            fmt.b_packetized = false;

            p_extra1 = MyRTSPClient::parseH264ConfigStr( sub->fmtp_spropvps(), i_extra1 );
            p_extra2 = MyRTSPClient::parseH264ConfigStr( sub->fmtp_spropsps(), i_extra2 );
            p_extra3 = MyRTSPClient::parseH264ConfigStr( sub->fmtp_sproppps(), i_extra3 );
            i_extraTot = i_extra1 + i_extra2 + i_extra3;
            if( i_extraTot > 0 )
            {
                fmt.i_extra = i_extraTot;
                fmt.p_extra = new uint8_t[ i_extraTot ];
                if( p_extra1 )
                {
                    memcpy( fmt.p_extra, p_extra1, i_extra1 );
                }
                if( p_extra2 )
                {
                    memcpy( fmt.p_extra + i_extra1, p_extra2, i_extra2 );
                }
                if( p_extra3 )
                {
                    memcpy( fmt.p_extra+i_extra1+i_extra2, p_extra3, i_extra3 );
                }

                delete[] p_extra1; delete[] p_extra2; delete[] p_extra3;
            }
        }
#endif
        else if( !strcmp( sub->codecName(), "JPEG" ) )
        {
            fmt.i_codec = VLC_CODEC_MJPG;
        }
        else if( !strcmp( sub->codecName(), "MP4V-ES" ) )
        {
            unsigned int i_extra;
            uint8_t      *p_extra;

            fmt.i_codec = VLC_CODEC_MP4V;

            if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                    i_extra ) ) )
            {
                fmt.i_extra = i_extra;
                fmt.p_extra = new uint8_t[ i_extra ];
                memcpy( fmt.p_extra, p_extra, i_extra );
                delete[] p_extra;
            }
        }
        else if( !strcmp( sub->codecName(), "X-QT" ) ||
                    !strcmp( sub->codecName(), "X-QUICKTIME" ) ||
                    !strcmp( sub->codecName(), "X-QDM" ) ||
                    !strcmp( sub->codecName(), "X-SV3V-ES" )  ||
                    !strcmp( sub->codecName(), "X-SORENSONVIDEO" ) )
        {
            b_quicktime = true;
        }
        else if( !strcmp( sub->codecName(), "MP2T" ) )
        {
            b_muxed = true;
            //tk->p_out_muxed = stream_DemuxNew( p_demux, "ts", p_demux->out );
        }
        else if( !strcmp( sub->codecName(), "MP2P" ) ||
                    !strcmp( sub->codecName(), "MP1S" ) )
        {
            b_muxed = true;
            /*tk->p_out_muxed = stream_DemuxNew( p_demux, "ps",
                                                p_demux->out );*/
        }
        else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
        {
            b_asf = true;
            /*if( p_sys->p_out_asf == NULL )
                p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
                                                    p_demux->out );;*/
        }
        else if( !strcmp( sub->codecName(), "DV" ) )
        {
            b_muxed = true;
            b_discard_trunc = true;
            /*p_out_muxed = stream_DemuxNew( p_demux, "rawdv",
                                                p_demux->out );*/
        }
        else if( !strcmp( sub->codecName(), "VP8" ) )
        {
            fmt.i_codec = VLC_CODEC_VP8;
        }
        else if( !strcmp( sub->codecName(), "THEORA" ) )
        {
            fmt.i_codec = VLC_CODEC_THEORA;
            unsigned int i_extra;
            unsigned char *p_extra;
            if( ( p_extra = MyRTSPClient::parseVorbisConfigStr( sub->fmtp_config(),
                                                i_extra ) ) )
            {
                fmt.i_extra = i_extra;
                fmt.p_extra = p_extra;
            }
            //else
            //    msg_Warn( p_demux,"Missing or unsupported theora header." );
        }
    }
    //else if( !strcmp( sub->mediumName(), "text" ) )
    //{
    //    es_format_Init( &tk->fmt, SPU_ES, VLC_FOURCC('u','n','d','f') );

    //    if( !strcmp( sub->codecName(), "T140" ) )
    //    {
    //        tk->fmt.i_codec = VLC_CODEC_ITU_T140;
    //    }
    //}

    ///* Try and parse a=lang: attribute */
    //p_lang = strstr( sub->savedSDPLines(), "a=lang:" );
    //if( !p_lang )
    //    p_lang = p_sess_lang;

    //if( p_lang )
    //{
    //    unsigned i_lang_len;
    //    p_lang += 7;
    //    i_lang_len = strcspn( p_lang, " \r\n" );
    //    tk->fmt.psz_language = strndup( p_lang, i_lang_len );
    //}

    if( !b_quicktime && !b_muxed && !b_asf )
    {
        //tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
		b_selected = true;
    }

    if( sub->rtcpInstance() != NULL )
    {
        sub->rtcpInstance()->setByeHandler( Live555Client::LiveTrack::streamClose, this );
    }

    //if( tk->p_es || tk->b_quicktime || ( tk->b_muxed && tk->p_out_muxed ) ||
    //    ( tk->b_asf && p_sys->p_out_asf ) )
    //{
    //    TAB_APPEND_CAST( (live_track_t **), p_sys->i_track, p_sys->track, tk );
    //}
    //else
    //{
    //    /* BUG ??? */
    //    msg_Err( p_demux, "unusable RTSP track. this should not happen" );
    //    es_format_Clean( &tk->fmt );
    //    free( tk );
    //}

	return 0;
}

void Live555Client::LiveTrack::streamRead(void *opaque, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration)
{
	Live555Client::LiveTrack* pThis = static_cast<Live555Client::LiveTrack*>(opaque);
	pThis->parent->onStreamRead(pThis, i_size, i_truncated_bytes, pts, duration);
}

void Live555Client::LiveTrack::streamClose(void* opaque)
{
	Live555Client::LiveTrack* pThis = static_cast<Live555Client::LiveTrack*>(opaque);
	pThis->parent->onStreamClose(pThis);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void Live555Client::taskInterruptData( void *opaque )
{
	Live555Client *pThis = static_cast<Live555Client*>(opaque);

	pThis->i_no_data_ti++;

    /* Avoid lock */
    pThis->event_data = (char)0xff;
}

void Live555Client::taskInterruptRTSP( void *opaque )
{
	Live555Client *pThis = static_cast<Live555Client*>(opaque);

    /* Avoid lock */
    pThis->event_rtsp = (char)0xff;
}

bool Live555Client::waitLive555Response( int i_timeout /* ms */ )
{
	TaskToken task;
	BasicTaskScheduler* sch = (BasicTaskScheduler*)scheduler;
    event_rtsp = 0;
    if( i_timeout > 0 )
    {
        /* Create a task that will be called if we wait more than timeout ms */
        task = sch->scheduleDelayedTask( i_timeout*1000,
                                                      taskInterruptRTSP,
                                                      this );
    }
    event_rtsp = 0;
    b_error = true;
    i_live555_ret = 0;
    sch->doEventLoop( &event_rtsp );
    //here, if b_error is true and i_live555_ret = 0 we didn't receive a response
    if( i_timeout > 0 )
    {
        /* remove the task */
        sch->unscheduleDelayedTask( task );
    }
    return !b_error;
}

#define DEFAULT_FRAME_BUFFER_SIZE 500000

int Live555Client::setup()
{
	MediaSubsessionIterator *iter   = NULL;
    MediaSubsession         *sub    = NULL;
	MediaSession            *ms     = NULL;
	RTSPClient* client = static_cast<RTSPClient*>(rtsp);
	UsageEnvironment* environment = static_cast<UsageEnvironment*>(env);

	bool           b_rtsp_tcp;
    int            i_client_port;
    int            i_return = 0;
    unsigned int   i_receive_buffer = 0;
    int            i_frame_buffer = DEFAULT_FRAME_BUFFER_SIZE;
    unsigned const thresh = 200000; /* RTP reorder threshold .2 second (default .1) */
    //const char     *p_sess_lang = NULL;
    //const char     *p_lang;

    b_rtsp_tcp    = false; /*var_CreateGetBool( p_demux, "rtsp-tcp" ) ||
                    var_GetBool( p_demux, "rtsp-http" );*/
    i_client_port = u_port_begin; //var_InheritInteger( p_demux, "rtp-client-port" );

	/* here print sdp on debug */
	printf("SDP content:\n%s", sdp.c_str());
    /* Create the session from the SDP */
	if( !( media_session = ms = MediaSession::createNew( *environment, sdp.c_str() ) ) )
    {
        return -1;
    }

	//if (sdp.compare( "m=" ) != 0 )
	//{
	//}
	iter = new MediaSubsessionIterator( *ms );
	while( ( sub = iter->next() ) != NULL )
	{
		bool b_init;
		LiveTrack* tk;
		/* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
            i_receive_buffer = 100000;
        else if( !strcmp( sub->mediumName(), "video" ) )
            i_receive_buffer = 2000000;
        else if( !strcmp( sub->mediumName(), "text" ) )
            ;
        else continue;

		if( i_client_port != -1 )
        {
            sub->setClientPortNum( i_client_port );
            i_client_port += 2;
        }

        if( strcasestr( sub->codecName(), "REAL" ) )
        {
            //msg_Info( p_demux, "real codec detected, using real-RTSP instead" );
            //b_real = true; /* This is a problem, we'll handle it later */
            continue;
        }

        if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
            b_init = sub->initiate( 0 );
        else
            b_init = sub->initiate();

        if( b_init )
        {
            if( sub->rtpSource() != NULL )
            {
                int fd = sub->rtpSource()->RTPgs()->socketNum();

                /* Increase the buffer size */
                if( i_receive_buffer > 0 )
                    increaseReceiveBufferTo( *environment, fd, i_receive_buffer );

                /* Increase the RTP reorder timebuffer just a bit */
                sub->rtpSource()->setPacketReorderingThresholdTime(thresh);
            }
            //msg_Dbg( p_demux, "RTP subsession '%s/%s'", sub->mediumName(),
            //         sub->codecName() );

            /* Issue the SETUP */
            if( client )
            {
                client->sendSetupCommand( *sub, MyRTSPClient::default_live555_callback, False,
                                               toBool( b_rtsp_tcp ),
                                               False/*toBool( p_sys->b_force_mcast && !b_rtsp_tcp )*/ );
                if( !waitLive555Response() )
                {
                    /* if we get an unsupported transport error, toggle TCP
                     * use and try again */
                    if( i_live555_ret == 461 )
                        client->sendSetupCommand( *sub, MyRTSPClient::default_live555_callback, False,
                                                       !toBool( b_rtsp_tcp ), False );
                    if( i_live555_ret != 461 || !waitLive555Response() )
                    {
                        //msg_Err( p_demux, "SETUP of'%s/%s' failed %s",
                        //         sub->mediumName(), sub->codecName(),
                        //         p_sys->env->getResultMsg() );
                        continue;
                    }
                    else
                    {
                        //var_SetBool( p_demux, "rtsp-tcp", true );
                        b_rtsp_tcp = true;
                    }
                }
            }

            /* Check if we will receive data from this subsession for
             * this track */
            if( sub->readSource() == NULL ) continue;
            //if( !p_sys->b_multicast )
            //{
            //    /* We need different rollover behaviour for multicast */
            //    p_sys->b_multicast = IsMulticastAddress( sub->connectionEndpointAddress() );
            //}

			tk = new LiveTrack(this, sub, i_frame_buffer);// (live_track_t*)malloc( sizeof( live_track_t ) );
            if( !tk )
            {
                delete iter;
                return -1;//VLC_ENOMEM;
            }

			if (tk->init())
			{
				delete tk;
			}
			else {
				listTracks.push_back(tk);
				onInitializedTrack(tk);
			}
        }
    }
    delete iter;
    
	if( !listTracks.size() ) i_return = -1;

    /* Retrieve the starttime if possible */
    f_npt_start = ms->playStartTime();

    /* Retrieve the duration if possible */
    f_npt_length = ms->playEndTime();

    /* */
    //msg_Dbg( p_demux, "setup start: %f stop:%f", p_sys->f_npt_start, p_sys->f_npt_length );

    /* */
    b_no_data = true;
    i_no_data_ti = 0;

	u_port_begin = i_client_port;

    return i_return;
}

void Live555Client::controlPauseState()
{
	RTSPClient* client = static_cast<RTSPClient*>(rtsp);
	MediaSession* ms = static_cast<MediaSession*>(media_session);

	b_is_paused = !b_is_paused;

	if (b_is_paused) {
		client->sendPauseCommand( *ms, MyRTSPClient::default_live555_callback );
	}
	else {
		client->sendPlayCommand( *ms, MyRTSPClient::default_live555_callback, -1.0f, -1.0f, ms->scale() );
	}

    waitLive555Response();
}

int Live555Client::demux(void)
{
    TaskToken      task;
	RTSPClient*    client = static_cast<RTSPClient*>(rtsp);
	MediaSession*  ms = static_cast<MediaSession*>(media_session);
	TaskScheduler* sch = static_cast<TaskScheduler*>(scheduler);

    bool            b_send_pcr = true;
    //int             i;

    /* Check if we need to send the server a Keep-A-Live signal */
    if( b_timeout_call && client && ms )
    {
        char *psz_bye = NULL;
        client->sendGetParameterCommand( *ms, NULL, psz_bye );
        b_timeout_call = false;
    }

	if (b_is_paused)
		return 1;

    /* First warn we want to read data */
    event_data = 0;

	std::size_t numIdle = 0;
    for (auto it = listTracks.begin(); it != listTracks.end(); ++it)
    {
        LiveTrack *tk = *it;

		if (!tk->isSelected()) {
			numIdle ++;
			continue;
		}

		MediaSubsession* sub = static_cast<MediaSubsession*>(tk->getMediaSubsession());
		uint8_t* p_buffer = tk->buffer();

		if( tk->getFormat().i_codec == VLC_CODEC_AMR_NB ||
			tk->getFormat().i_codec == VLC_CODEC_AMR_WB )
		{
			p_buffer++;
		}
		else if( tk->getFormat().i_codec == VLC_CODEC_H261 || tk->getFormat().i_codec == VLC_CODEC_H264 || tk->getFormat().i_codec == VLC_CODEC_HEVC )
		{
			p_buffer += 4;
		}

        if( !tk->isWaiting() )
        {
            tk->doWaiting(1);
            sub->readSource()->getNextFrame( p_buffer, tk->buffer_size(),
                                          Live555Client::LiveTrack::streamRead, tk, Live555Client::LiveTrack::streamClose, tk );
        }
    }

	/* Check if no track is available */
	if (numIdle == listTracks.size())
		b_need_stop = true;
    
	/* Create a task that will be called if we wait more than 300ms */
    task = sch->scheduleDelayedTask( 300000, taskInterruptData, this );

    /* Do the read */
    sch->doEventLoop( &event_data );

    /* remove the task */
    sch->unscheduleDelayedTask( task );

    /* Check for gap in pts value */
  //  for (auto it = listTracks.begin(); it != listTracks.end(); ++it)
  //  {
  //      LiveTrack *tk = *it;
		//MediaSubsession* sub = static_cast<MediaSubsession*>(tk->getMediaSubsession());

  //      if( !tk->b_muxed && !tk->b_rtcp_sync &&
  //          sub->rtpSource() && sub->rtpSource()->hasBeenSynchronizedUsingRTCP() )
  //      {
  //          msg_Dbg( p_demux, "tk->rtpSource->hasBeenSynchronizedUsingRTCP()" );

  //          es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
  //          tk->b_rtcp_sync = true;
  //          /* reset PCR */
  //          tk->i_pts = VLC_TS_INVALID;
  //          tk->f_npt = 0.;
  //          p_sys->i_pcr = 0;
  //          p_sys->f_npt = 0.;
  //      }
  //  }

  //  if( p_sys->b_multicast && p_sys->b_no_data &&
  //      ( p_sys->i_no_data_ti > 120 ) )
  //  {
  //      /* FIXME Make this configurable
  //      msg_Err( p_demux, "no multicast data received in 36s, aborting" );
  //      return 0;
  //      */
  //  }
  //  else if( !p_sys->b_multicast && !p_sys->b_paused &&
  //            p_sys->b_no_data && ( p_sys->i_no_data_ti > 34 ) )
  //  {
  //      bool b_rtsp_tcp = var_GetBool( p_demux, "rtsp-tcp" ) ||
  //                              var_GetBool( p_demux, "rtsp-http" );

  //      if( !b_rtsp_tcp && p_sys->rtsp && p_sys->ms )
  //      {
  //          msg_Warn( p_demux, "no data received in 10s. Switching to TCP" );
  //          if( RollOverTcp( p_demux ) )
  //          {
  //              msg_Err( p_demux, "TCP rollover failed, aborting" );
  //              return 0;
  //          }
  //          return 1;
  //      }
  //      msg_Err( p_demux, "no data received in 10s, aborting" );
  //      return 0;
  //  }

	if( i_no_data_ti > 33 || i_live555_ret == 454) //no data received in 10s, eof ?
    {
		onEOF();
        return 0;
    }

    return b_error ? 0 : 1;
}

void Live555Client::demux_loop(void* opaque)
{
	Live555Client* pThis = static_cast<Live555Client*>(opaque);
	std::chrono::high_resolution_clock::time_point last_call_timeout= std::chrono::high_resolution_clock::now();

#if defined(_WIN32) || defined(WIN32)
	timeBeginPeriod(1);
#endif

	while (pThis->demuxLoopFlag)
	{
		std::chrono::seconds lasting = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - last_call_timeout);
		if (lasting.count() >= (pThis->i_timeout - 2)) {
			last_call_timeout= std::chrono::high_resolution_clock::now();
			pThis->b_timeout_call = true;
		}

		if (pThis->b_do_control_pause_state) {
			pThis->controlPauseState();
			pThis->b_do_control_pause_state = false;
		}

		pThis->demux();
	}

#if defined(_WIN32) || defined(WIN32)
	timeBeginPeriod(1);
#endif

}

Live555Client::Live555Client(void)
	: env(NULL)
	, scheduler(NULL)
	, rtsp(NULL)
	, media_session(NULL)
	, event_rtsp(0)
	, event_data(0)
	, b_error(false)
	, b_get_param(false)
	, i_live555_ret(0)
	, i_timeout(60)
	, b_timeout_call(false)
	, i_pcr(VLC_TS_0)
	, f_npt(0)
	, f_npt_length(0)
	, f_npt_start(0)
	, b_no_data(false)
	, i_no_data_ti(0)
	, b_need_stop(false)
	, b_is_paused(false)
	, b_do_control_pause_state(false)
	, u_port_begin(0)
	, user_name("")
	, password("")
	, sdp("")
	, user_agent("CCTVClient/1.0")
	, demuxLoopFlag(true)
	, demuxLoopHandle(NULL)
	, b_is_playing(false)
{
}


Live555Client::~Live555Client(void)
{
	stop();
}


int Live555Client::open(const char* url)
{
	Authenticator authenticator;
	UsageEnvironment* environment;
	RTSPClient* client;
	scheduler = BasicTaskScheduler::createNew();
	if (!scheduler)
		return -1;

	TaskScheduler* sch = static_cast<TaskScheduler*>(scheduler);
	env = BasicUsageEnvironment::createNew(*sch);
	if (!env) {
		stop();
		return -1;
	}

	environment = static_cast<UsageEnvironment*>(env);

	rtsp = client = new MyRTSPClient(*environment, url, 0, user_agent.c_str(), 0, this);
	if (!rtsp) {
		stop();
		return -1;
	}

	authenticator.setUsernameAndPassword( user_name.c_str(), password.c_str() );
	client->sendOptionsCommand( &MyRTSPClient::continueAfterOPTIONS, &authenticator );
	if( !waitLive555Response( 5000 ) )
    {
        int i_code = i_live555_ret;
        if( i_code == 401 )
        {
			// authentication failed
            // free up all resource
			// set user name and password and open again
			stop();
			return i_code;
        }
        else
        {
            stop();

			if (i_code > 0)
				return i_code;
			else
				return -1; // timeout
        }
    }

	f_npt_start = 0;

	return setup();
}


int Live555Client::play()
{
	RTSPClient* client = static_cast<RTSPClient*>(rtsp);
	MediaSession* ms = static_cast<MediaSession*>(media_session);

    if( client )
    {
        /* The PLAY */
        client->sendPlayCommand( *ms, MyRTSPClient::default_live555_callback, f_npt_start, -1, 1 );

        if( !waitLive555Response() )
        {
            //msg_Err( p_demux, "RTSP PLAY failed %s", p_sys->env->getResultMsg() );
            return -1;//VLC_EGENERIC;
        }

		if (i_live555_ret)
			return i_live555_ret;

        /* Retrieve the timeout value and set up a timeout prevention thread */
        i_timeout = client->sessionTimeoutParameter();
        if( i_timeout <= 0 )
            i_timeout = 60; /* default value from RFC2326 */

        ///* start timeout-thread only if GET_PARAMETER is supported by the server */
        ///* or start it if wmserver dialect, since they don't report that GET_PARAMETER is supported correctly */
        //if( !p_sys->p_timeout && ( p_sys->b_get_param || var_InheritBool( p_demux, "rtsp-wmserver" ) ) )
        //{
        //    msg_Dbg( p_demux, "We have a timeout of %d seconds",  p_sys->i_timeout );
        //    p_sys->p_timeout = (timeout_thread_t *)malloc( sizeof(timeout_thread_t) );
        //    if( p_sys->p_timeout )
        //    {
        //        memset( p_sys->p_timeout, 0, sizeof(timeout_thread_t) );
        //        p_sys->p_timeout->p_sys = p_demux->p_sys; /* lol, object recursion :D */
        //        if( vlc_clone( &p_sys->p_timeout->handle,  TimeoutPrevention,
        //                       p_sys->p_timeout, VLC_THREAD_PRIORITY_LOW ) )
        //        {
        //            msg_Err( p_demux, "cannot spawn liveMedia timeout thread" );
        //            free( p_sys->p_timeout );
        //            p_sys->p_timeout = NULL;
        //        }
        //        else
        //            msg_Dbg( p_demux, "spawned timeout thread" );
        //    }
        //    else
        //        msg_Err( p_demux, "cannot spawn liveMedia timeout thread" );
        //}
    }
    i_pcr = 0;

    /* Retrieve the starttime if possible */
    f_npt_start = ms->playStartTime();
    if( ms->playEndTime() > 0 )
        f_npt_length = ms->playEndTime();

	// now create thread for get data
	b_need_stop = false;
	b_error = false;
	b_is_paused = false;
	b_do_control_pause_state = false;
	b_timeout_call = true;
	b_is_playing = true;

	if (!demuxLoopHandle) {
		demuxLoopFlag = true;
		demuxLoopHandle = new std::thread(demux_loop, this);
	}
    return 0;
}

void Live555Client::togglePause()
{
	b_do_control_pause_state = true;
}

int Live555Client::stop()
{
	if (demuxLoopHandle) {
		demuxLoopFlag = false;
		demuxLoopHandle->join();
		delete demuxLoopHandle;
		demuxLoopHandle = NULL;
	}

	RTSPClient* client = static_cast<RTSPClient*>(rtsp);
	MediaSession* ms = static_cast<MediaSession*>(media_session);
	UsageEnvironment* environment = static_cast<UsageEnvironment*>(env);
	TaskScheduler* sch = static_cast<TaskScheduler*>(scheduler);

    if( client && ms ) 
		client->sendTeardownCommand( *ms, NULL );

    if( ms ) {
		Medium::close( ms );
		media_session = NULL;
	}

	if( client ) {
		RTSPClient::close( client );
		rtsp = NULL;
	}

    if( env ) {
		environment->reclaim();
		env = NULL;
	}

	while (!listTracks.empty()) {
		delete listTracks.front();
		listTracks.erase(listTracks.begin());
	}
    
	delete sch;
	scheduler = NULL;
	b_is_playing = false;
	u_port_begin = 0;
	user_name = "";
	password = "";

	return 0;
}

void Live555Client::setUser(const char* user_name, const char* password)
{
	this->user_name = user_name;
	this->password = password;
}

void Live555Client::setUserAgent(const char* user_agent)
{
	this->user_agent = user_agent;
}

void Live555Client::continueAfterDESCRIBE( int result_code, char* result_string )
{
    i_live555_ret = result_code;
    if ( result_code == 0 )
    {
        char* sdpDescription = result_string;
        if( sdpDescription )
        {
            sdp = std::string( sdpDescription );
            b_error = false;
        }
    }
    else
        b_error = true;
    event_rtsp = 1;
}

void Live555Client::continueAfterOPTIONS( int result_code, char* result_string )
{
    b_get_param =
      // If OPTIONS fails, assume GET_PARAMETER is not supported but
      // still continue on with the stream.  Some servers (foscam)
      // return 501/not implemented for OPTIONS.
      result_code == 0
      && result_string != NULL
      && strstr( result_string, "GET_PARAMETER" ) != NULL;

	RTSPClient* client = static_cast<RTSPClient*>(rtsp);
    client->sendDescribeCommand( MyRTSPClient::continueAfterDESCRIBE );
}

void Live555Client::live555Callback( int result_code )
{
    i_live555_ret = result_code;
    b_error = i_live555_ret != 0;
    event_rtsp = 1;
}

void Live555Client::onStreamRead(LiveTrack* track, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration )
{
	MediaSubsession* sub = static_cast<MediaSubsession*>(track->getMediaSubsession());

    int64_t i_pts = (int64_t)pts.tv_sec * INT64_C(1000000) +
        (int64_t)pts.tv_usec;

	int64_t i_dts;

    /* XXX Beurk beurk beurk Avoid having negative value XXX */
    i_pts &= INT64_C(0x00ffffffffffffff);

    /* Retrieve NPT for this pts */
    track->setNPT(sub->getNormalPlayTime(pts));

    if( track->isQuicktime() /* && tk->p_es == NULL */)
    {
        QuickTimeGenericRTPSource *qtRTPSource =
            (QuickTimeGenericRTPSource*)sub->rtpSource();
        QuickTimeGenericRTPSource::QTState &qtState = qtRTPSource->qtState;
        uint8_t *sdAtom = (uint8_t*)&qtState.sdAtom[4];

        /* Get codec information from the quicktime atoms :
         * http://developer.apple.com/quicktime/icefloe/dispatch026.html */
        if( track->getFormat().i_cat == VIDEO_ES ) {
            if( qtState.sdAtomSize < 16 + 32 )
            {
                /* invalid */
                event_data = (char)0xff;
                track->doWaiting(0);
                return;
            }
            track->getFormat().i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
            track->getFormat().video.i_width  = (sdAtom[28] << 8) | sdAtom[29];
            track->getFormat().video.i_height = (sdAtom[30] << 8) | sdAtom[31];

            if( track->getFormat().i_codec == VLC_FOURCC('a', 'v', 'c', '1') )
            {
                uint8_t *pos = (uint8_t*)qtRTPSource->qtState.sdAtom + 86;
                uint8_t *endpos = (uint8_t*)qtRTPSource->qtState.sdAtom
                                  + qtRTPSource->qtState.sdAtomSize;
                while (pos+8 < endpos) {
                    unsigned int atomLength = pos[0]<<24 | pos[1]<<16 | pos[2]<<8 | pos[3];
                    if( atomLength == 0 || atomLength > (unsigned int)(endpos-pos)) break;
                    if( memcmp(pos+4, "avcC", 4) == 0 &&
                        atomLength > 8 &&
                        atomLength <= INT_MAX )
                    {
                    	track->getFormat().i_extra = atomLength-8;
                    	track->getFormat().p_extra = new uint8_t[ track->getFormat().i_extra ];
                        memcpy(track->getFormat().p_extra, pos+8, atomLength-8);
                        break;
                    }
                    pos += atomLength;
                }
            }
            else
            {
            	track->getFormat().i_extra        = qtState.sdAtomSize - 16;
            	track->getFormat().p_extra        = new uint8_t[ track->getFormat().i_extra ];
                memcpy( track->getFormat().p_extra, &sdAtom[12], track->getFormat().i_extra );
            }
        }
        else {
            if( qtState.sdAtomSize < 24 )
            {
                /* invalid */
                event_data = (char)0xff;
                track->doWaiting(0);
                return;
            }
            track->getFormat().i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
            track->getFormat().audio.i_bitspersample = (sdAtom[22] << 8) | sdAtom[23];
        }
        //tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
    }

    /* grow buffer if it looks like buffer is too small, but don't eat
     * up all the memory on strange streams */
    if( i_truncated_bytes > 0 )
    {
        if( track->buffer_size() < 2000000 )
        {
            //void *p_tmp;
            //msg_Dbg( p_demux, "lost %d bytes", i_truncated_bytes );
            //msg_Dbg( p_demux, "increasing buffer size to %d", tk->i_buffer * 2 );
            //p_tmp = realloc( tk->p_buffer, tk->i_buffer * 2 );
            //if( p_tmp == NULL )
            //{
            //    msg_Warn( p_demux, "realloc failed" );
            //}
            //else
            //{
            //    tk->p_buffer = (uint8_t*)p_tmp;
            //    tk->i_buffer *= 2;
            //}
        }

        //if( tk->b_discard_trunc )
        //{
        //    event_data = 0xff;
        //    track->doWaiting(0);
        //    return;
        //}
    }

    assert( i_size <= track->buffer_size() );
	unsigned int out_size = i_size;

	if( track->getFormat().i_codec == VLC_CODEC_AMR_NB ||
        track->getFormat().i_codec == VLC_CODEC_AMR_WB )
    {
        AMRAudioSource *amrSource = (AMRAudioSource*)sub->readSource();

        track->buffer()[0] = amrSource->lastFrameHeader();
        out_size++;
    }
    else if( track->getFormat().i_codec == VLC_CODEC_H261 )
    {
        H261VideoRTPSource *h261Source = (H261VideoRTPSource*)sub->rtpSource();
        uint32_t header = h261Source->lastSpecialHeader();
        memcpy( track->buffer(), &header, 4 );
        out_size += 4;

        //if( sub->rtpSource()->curPacketMarkerBit() )
        //    p_block->i_flags |= BLOCK_FLAG_END_OF_FRAME;
    }
    else if( track->getFormat().i_codec == VLC_CODEC_H264 || track->getFormat().i_codec == VLC_CODEC_HEVC )
    {
        /* Normal NAL type */
        track->buffer()[0] = 0x00;
        track->buffer()[1] = 0x00;
        track->buffer()[2] = 0x00;
        track->buffer()[3] = 0x01;
		out_size += 4;
    }
    //else if( tk->b_asf )
    //{
    //    p_block = StreamParseAsf( p_demux, tk,
    //                              tk->sub->rtpSource()->curPacketMarkerBit(),
    //                              tk->p_buffer, i_size );
    //}
    //else
    //{
    //    p_block = block_Alloc( i_size );
    //    memcpy( p_block->p_buffer, tk->p_buffer, i_size );
    //}

    if( i_pcr < i_pts )
    {
        i_pcr = i_pts;
    }

    /* Update our global npt value */
    if( track->getNPT() > 0 &&
        ( track->getNPT() < f_npt_length || f_npt_length <= 0 ) )
        f_npt = track->getNPT();

	i_dts = ( track->getFormat().i_codec == VLC_CODEC_MPGV ) ? VLC_TS_INVALID : (VLC_TS_0 + i_pts);

	onData(track, track->buffer(), out_size, i_truncated_bytes, i_pts, i_dts);

    /* warn that's ok */
    event_data = (char)0xff;

    /* we have read data */
    track->doWaiting(0);
    b_no_data = false;
    i_no_data_ti = 0;

    //if( i_pts > 0 && !tk->b_muxed )
    //{
    //    tk->i_pts = i_pts;
    //}
	{
		MediaSubsession* sub = static_cast<MediaSubsession*>(track->getMediaSubsession());
		uint8_t* p_buffer = track->buffer();

		if( track->getFormat().i_codec == VLC_CODEC_AMR_NB ||
			track->getFormat().i_codec == VLC_CODEC_AMR_WB )
		{
			p_buffer++;
		}
		else if( track->getFormat().i_codec == VLC_CODEC_H261 || track->getFormat().i_codec == VLC_CODEC_H264 || track->getFormat().i_codec == VLC_CODEC_HEVC )
		{
			p_buffer += 4;
		}

        track->doWaiting(1);
        sub->readSource()->getNextFrame( p_buffer, track->buffer_size(),
                                        Live555Client::LiveTrack::streamRead, track, Live555Client::LiveTrack::streamClose, track);
	}
}

void Live555Client::onStreamClose(LiveTrack* track)
{
	track->setSelected(false);
    event_rtsp = (char)0xff;
    event_data = (char)0xff;

	int nb_streams = 0;
	for (auto it = listTracks.begin(); it != listTracks.end(); ++it)
	{
		if ((*it)->isSelected())
			nb_streams++;
	}

	//have no streams left, it is realy EOF
	if (!nb_streams)
		onEOF();
}
