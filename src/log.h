///////////////////////////////////////////////////////////////////////////////
//
//  The contents of this file are subject to the Mozilla Public License
//  Version 1.1 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.mozilla.org/MPL/
//
//  Software distributed under the License is distributed on an "AS IS"
//  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
//  License for the specific language governing rights and limitations
//  under the License.
// 
//  The Original Code is MP4v2.
// 
//  The Initial Developer of the Original Code is David Byron.
//  Portions created by David Byron are Copyright (C) 2009, 2010, 2011.
//  All Rights Reserved.
//
//  Contributors:
//      David Byron, dbyron@dbyron.com
//
///////////////////////////////////////////////////////////////////////////////

#ifndef MP4V2_IMPL_LOG_H
#define MP4V2_IMPL_LOG_H

namespace std {
    std::string to_string(MP4LogLevel level);
}

namespace mp4v2 { namespace impl {

///////////////////////////////////////////////////////////////////////////////

/**
 * Handle logging either to standard out or to a callback
 * function
 */
class MP4V2_EXPORT Log {
private:
    MP4LogLevel                 _verbosity;
    MP4LogCallback              _cb_func;
    void*                       _handle;

public:
    const MP4LogLevel&          verbosity;

public:
    Log( MP4LogLevel = MP4_LOG_NONE );
    virtual ~Log();

    void setLogCallback ( MP4LogCallback callback, void* handle );

    void setVerbosity   ( MP4LogLevel );

    void errorf ( const char* format, ... ) MP4V2_WFORMAT_PRINTF(2,3);
    void warningf ( const char* format, ... ) MP4V2_WFORMAT_PRINTF(2,3);
    void infof ( const char* format, ... ) MP4V2_WFORMAT_PRINTF(2,3);
    void verbose1f ( const char* format, ... ) MP4V2_WFORMAT_PRINTF(2,3);
    void verbose2f ( const char* format, ... ) MP4V2_WFORMAT_PRINTF(2,3);
    void verbose3f ( const char* format, ... ) MP4V2_WFORMAT_PRINTF(2,3);
    void verbose4f ( const char* format, ... ) MP4V2_WFORMAT_PRINTF(2,3);

    void dump ( uint8_t       indent,
                MP4LogLevel   verbosity_,
                const char*   format, ... ) MP4V2_WFORMAT_PRINTF(4,5);
    void vdump ( uint8_t       indent,
                 MP4LogLevel   verbosity_,
                 const char*   format, va_list ap );
    void printf ( MP4LogLevel   verbosity_,
                  const char*   format, ... ) MP4V2_WFORMAT_PRINTF(3,4);
    void vprintf ( MP4LogLevel  verbosity_,
                   const char*  format, va_list ap );

    void hexDump ( uint8_t              indent,
                   MP4LogLevel          verbosity_,
                   const uint8_t*       pBytes,
                   uint32_t             numBytes,
                   const char*          format, ... ) MP4V2_WFORMAT_PRINTF(6,7);

    void errorf ( const Exception&      x );

    std::string formatMsg(const char* format, va_list args);
    std::string formatMsg(const std::string& category, const std::string& location, const char* format, ...);
    std::string formatTrackMsg(const std::string& category, const std::string& location, MP4TrackId trackID, const char* format, ...);

private:
    Log ( const Log &src );
    Log &operator= ( const Log &src );
};

/**
 * A global (at least to mp4v2) log object for code that
 * needs to log something but doesn't otherwise have access
 * to one
 */
extern Log log;
///////////////////////////////////////////////////////////////////////////////

// Error categories
#define SPECIFICATION_ERROR                         (std::string("Specification"))
#define MALFORMED_ATOM_ERROR(atom)                  (std::string("Malformed atom '") + atom + "'")
#define MALFORMED_DESCRIPTOR_ERROR(tag)             (std::string("Malformed descriptor '") + tag + "'")
#define MISSING_ATOM_ERROR(atom)                    (std::string("Missing atom '") + atom + "'")
#define MISSING_PROPERTY_ERROR(property)            (std::string("Missing property '") + property + "'")
#define INVALID_TABLE_ENTRY_ERROR(table, index)     (std::string("Invalid table entry '") + table + "[" #index "]'")
#define INVALID_PROPERTY_VALUE_ERROR(property)      (std::string("Invalid property '") + property + "' value")
#define METADATA_MISMATCH_ERROR                     (std::string("Metadata mismatch"))
#define DURATION_ERROR                              (std::string("Duration error"))

// Error locations
#define CONTAINER_ERROR     (std::string("Container"))
#define TRACK_ERROR         (std::string("Track"))
#define VIDEO_ERROR         (std::string("Video"))
#define AUDIO_ERROR         (std::string("Audio"))
#define TIMECODE_ERROR      (std::string("Timecode"))
#define HINT_ERROR          (std::string("Hint Track"))
#define CAPTION_ERROR       (std::string("Captions"))
#define SUBTITLE_ERROR      (std::string("Subtitles"))
#define CHAPTER_ERROR       (std::string("Chapter Track"))

// [<Level>]: <Function>: <File>: <Message>
#define LOG_MSG(level, msg) \
{ \
    Logger().printf(level, "[%s]: %s: \"%s\": %s", std::to_string(level).c_str(), __FUNCTION__, __FILE__, msg); \
}
#define LOG_ERROR(msg) LOG_MSG(MP4_LOG_ERROR, msg)
 
// [<Level>]: <Function>: <File>: <Category>: <Location>: <Message>
#define LOG_FORMATTED_MSG(level, category, location, format, ...) \
{ \
    std::string _formattedMsg = Logger().formatMsg(category, location, format, ##__VA_ARGS__); \
    Logger().printf(level, "[%s]: %s: \"%s\": %s", std::to_string(level).c_str(), __FUNCTION__, __FILE__, _formattedMsg.c_str()); \
}
#define LOG_FORMATTED_ERROR(category, location, format, ...) \
    LOG_FORMATTED_MSG(MP4_LOG_ERROR, category, location, format, ##__VA_ARGS__)

// [<Level>]: <Function>: <File>: <Category>: <Location>: Track <Track ID>: <Error Message>
#define LOG_FORMATTED_TRACK_MSG(level, category, location, track, format, ...) \
{ \
    std::string _formattedMsg = Logger().formatTrackMsg(category, location, track, format, ##__VA_ARGS__); \
    Logger().printf(level, "[%s]: %s: \"%s\": %s", std::to_string(level).c_str(), __FUNCTION__, __FILE__, _formattedMsg.c_str()); \
}
#define LOG_FORMATTED_TRACK_ERROR(category, location, track, format, ...) \
    LOG_FORMATTED_TRACK_MSG(MP4_LOG_ERROR, category, location, track, format, ##__VA_ARGS__)

}} // namespace mp4v2::impl

#endif // MP4V2_IMPL_LOG_H
