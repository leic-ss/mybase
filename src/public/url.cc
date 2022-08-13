/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#include "url.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Converts a hex character to its integer value */
static char from_hex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
static char to_hex(char code) {
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(char *str)
{
    char *pstr = str, *buf = (char*)malloc(strlen(str) * 3 + 1), *pbuf = buf;
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') {
            *pbuf++ = *pstr;
        } /*else if (*pstr == ' ') {
            *pbuf++ = '+';
        } */ else {
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
        } 
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_decode(char *str)
{
    char *pstr = str, *buf = (char*)malloc(strlen(str) + 1), *pbuf = buf;
    while (*pstr) {
        if (*pstr == '%') {
          if (pstr[1] && pstr[2]) {
              *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
              pstr += 2;
          }
        } /*else if (*pstr == '+') { 
            *pbuf++ = ' ';
        } */ else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

std::string urlEncode(const std::string& url)
{
    std::string encoded_url;
    for (uint32_t i = 0; i < url.size(); i++) {
        char c = url.at(i);
        
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded_url.push_back(c);
        } /*else if (*pstr == ' ') {
            *pbuf++ = '+';
        } */ else {
            encoded_url.push_back('%');
            encoded_url.push_back( to_hex(c >> 4) );
            encoded_url.push_back( to_hex(c & 15 ) );
        }
    }

    return std::move(encoded_url);
}

std::string urlDecode(const std::string& url)
{
    std::string decoded_url;
    for (uint32_t i = 0; i < url.size(); i++) {
        char c = url.at(i);

        if (c == '%') {
          if ( (i+1) < url.size() && (i+2) < url.size() ) {
              decoded_url.push_back( from_hex( url.at(i+1) ) << 4 | from_hex( url.at(i+2) ) );
              i += 2;
          }
        } /*else if (*pstr == '+') { 
            *pbuf++ = ' ';
        } */ else {
            decoded_url.push_back(c);
        }
    }

    return std::move(decoded_url);
}