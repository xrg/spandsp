/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t35.c - ITU T.35 FAX non-standard facility processing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: t35.c,v 1.7 2004/11/05 14:48:40 steveu Exp $
 */

/*
 * The NSF data tables are adapted from the NSF handling in HylaFAX, which
 * carries the following copyright notice:
 *
 * Created by Dmitry Bely, April 2000
 * Copyright (c) 1994-1996 Sam Leffler
 * Copyright (c) 1994-1996 Silicon Graphics, Inc.
 * HylaFAX is a trademark of Silicon Graphics
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

/*! \file */

#include <stdint.h>
#include <ctype.h>

#include "spandsp/telephony.h"
#include "spandsp/t35.h"

#define NULL ((void *) 0)

#define T35_VENDOR_ID_LEN   3

typedef struct
{
    int model_id_size;
    const char *model_id;
    const char *model_name;
} model_data_t;

typedef struct
{
    const char *vendor_id;
    const char *vendor_name;
    int inverse_station_id_order;
    const model_data_t *known_models;
} nsf_data_t;

extern const char *t35_country_codes[256] =
{
    "Japan",                                    /* 0x00 */
    "Albania",
    "Algeria",
    "American Samoa",
    "Germany (Federal Republic of)",
    "Anguilla",
    "Antigua and Barbuda",
    "Argentina",
    "Ascension (see S. Helena)",
    "Australia",
    "Austria",
    "Bahamas",
    "Bahrain",
    "Bangladesh",
    "Barbados",
    "Belgium",
    "Belize",
    "Benin (Republic of)",
    "Bermudas",
    "Bhutan (Kingdom of)",
    "Bolivia",
    "Botswana",
    "Brazil",
    "British Antarctic Territory",
    "British Indian Ocean Territory",
    "British Virgin Islands",
    "Brunei Darussalam",
    "Bulgaria",
    "Myanmar (Union of)",
    "Burundi",
    "Byelorussia",
    "Cameroon",
    "Canada",                                   /* 0x20 */
    "Cape Verde",
    "Cayman Islands",
    "Central African Republic",
    "Chad",
    "Chile",
    "China",
    "Colombia",
    "Comoros",
    "Congo",
    "Cook Islands",
    "Costa Rica",
    "Cuba",
    "Cyprus",
    "Czech and Slovak Federal Republic",
    "Cambodia",
    "Democratic People's Republic of Korea",
    "Denmark",
    "Djibouti",
    "Dominican Republic",
    "Dominica",
    "Ecuador",
    "Egypt",
    "El Salvador",
    "Equatorial Guinea",
    "Ethiopia",
    "Falkland Islands",
    "Fiji",
    "Finland",
    "France",
    "French Polynesia",
    "French Southern and Antarctic Lands",
    "Gabon",                                        /* 0x40 */
    "Gambia",
    "Germany (Federal Republic of) ",
    "Angola",
    "Ghana",
    "Gibraltar",
    "Greece",
    "Grenada",
    "Guam",
    "Guatemala",
    "Guernsey",
    "Guinea",
    "Guinea-Bissau",
    "Guayana",
    "Haiti",
    "Honduras",
    "Hongkong",
    "Hungary (Republic of)",
    "Iceland",
    "India",
    "Indonesia",
    "Iran (Islamic Republic of)",
    "Iraq",
    "Ireland",
    "Israel",
    "Italy",
    "Côte d'Ivoire",
    "Jamaica",
    "Afghanistan",
    "Jersey",
    "Jordan",
    "Kenya",
    "Kiribati",                                     /* 0x60 */
    "Korea (Republic of)",
    "Kuwait",
    "Lao (People's Democratic Republic)",
    "Lebanon",
    "Lesotho",
    "Liberia",
    "Libya",
    "Liechtenstein",
    "Luxembourg",
    "Macau",
    "Madagascar",
    "Malaysia",
    "Malawi",
    "Maldives",
    "Mali",
    "Malta",
    "Mauritania",
    "Mauritius",
    "Mexico",
    "Monaco",
    "Mongolia",
    "Montserrat",
    "Morocco",
    "Mozambique",
    "Nauru",
    "Nepal",
    "Netherlands",
    "Netherlands Antilles",
    "New Caledonia",
    "New Zealand",
    "Nicaragua",
    "Niger",                                        /* 0x80 */
    "Nigeria",
    "Norway",
    "Oman",
    "Pakistan",
    "Panama",
    "Papua New Guinea",
    "Paraguay",
    "Peru",
    "Philippines",
    "Poland (Republic of)",
    "Portugal",
    "Puerto Rico",
    "Qatar",
    "Romania",
    "Rwanda",
    "Saint Kitts and Nevis",
    "Saint Croix",
    "Saint Helena and Ascension",
    "Saint Lucia",
    "San Marino",
    "Saint Thomas",
    "Sao Tomé and Principe",
    "Saint Vincent and the Grenadines",
    "Saudi Arabia",
    "Senegal",
    "Seychelles",
    "Sierra Leone",
    "Singapore",
    "Solomon Islands",
    "Somalia",
    "South Africa",
    "Spain",                                        /* 0xA0 */
    "Sri Lanka",
    "Sudan",
    "Suriname",
    "Swaziland",
    "Sweden",
    "Switzerland",
    "Syria",
    "Tanzania",
    "Thailand",
    "Togo",
    "Tonga",
    "Trinidad and Tobago",
    "Tunisia",
    "Turkey",
    "Turks and Caicos Islands",
    "Tuvalu",
    "Uganda",
    "Ukraine",
    "United Arab Emirates",
    "United Kingdom",
    "United States",
    "Burkina Faso",
    "Uruguay",
    "U.S.S.R.",
    "Vanuatu",
    "Vatican City State",
    "Venezuela",
    "Viet Nam",
    "Wallis and Futuna",
    "Western Samoa",
    "Yemen (Republic of)",
    "Yemen (Republic of)",                          /* 0xC0 */
    "Yugoslavia",
    "Zaire",
    "Zambia",
    "Zimbabwe"
};

static const model_data_t Canon[] =
{
    {5, "\x80\x00\x80\x48\x00", "Faxphone B640"},
    {5, "\x80\x00\x80\x49\x10", "Fax B100"},
    {5, "\x80\x00\x8A\x49\x10", "Laser Class 9000 Series"},
    {0}
};  

static const model_data_t Brother[] =
{
    {9, "\x55\x55\x00\x88\x90\x80\x5F\x00\x15\x51", "Intellifax 770"},
    {9, "\x55\x55\x00\x80\xB0\x80\x00\x00\x59\xD4", "Personal fax 190"},
    {9, "\x55\x55\x00\x8C\x90\x80\xF0\x02\x20", "MFC-8600"},
    {0}
};

static const model_data_t Panasonic0E[] =
{
    {10, "\x00\x00\x00\x96\x0F\x01\x02\x00\x10\x05\x02\x95\xC8\x08\x01\x49\x02\x41\x53\x54\x47", "KX-F90"},
    {10, "\x00\x00\x00\x96\x0F\x01\x03\x00\x10\x05\x02\x95\xC8\x08\x01\x49\x02                \x03", "KX-F230 or KX-FT21 or ..."},
    {10, "\x00\x00\x00\x16\x0F\x01\x03\x00\x10\x05\x02\x95\xC8\x08", "KX-F780"},
    {10, "\x00\x00\x00\x16\x0F\x01\x03\x00\x10\x00\x02\x95\x80\x08\x75\xB5", "KX-M260"},
    {10, "\x00\x00\x00\x16\x0F\x01\x02\x00\x10\x05\x02\x85\xC8\x08\xAD", "KX-F2050BS"},
    {0}
};

static const model_data_t Panasonic79[] =
{
    {10, "\x00\x00\x00\x02\x0F\x09\x12\x00\x10\x05\x02\x95\xC8\x88\x80\x80\x01", "UF-S10"},
    {10, "\x00\x00\x00\x16\x7F\x09\x13\x00\x10\x05\x16\x8D\xC0\xD0\xF8\x80\x01", "/Siemens Fax 940"},
    {10, "\x00\x00\x00\x16\x0F\x09\x13\x00\x10\x05\x06\x8D\xC0\x50\xCB", "Panafax UF-321"},
    {0}
};

static const model_data_t Ricoh[] =
{
    {10, "\x00\x00\x00\x12\x10\x0D\x02\x00\x50\x00\x2A\xB8\x2C", "/Nashuatec P394"},
    {0}
};

static const model_data_t Samsung8C[] =
{
    {4, "\x00\x00\x01\x00", "SF-2010"},
    {0}
};

static const model_data_t Sanyo[] =
{
    {10, "\x00\x00\x10\xB1\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x41\x26\xFF\xFF\x00\x00\x85\xA1", "SFX-107"},
    {10, "\x00\x00\x00\xB1\x12\xF2\x62\xB4\x82\x0A\xF2\x2A\x12\xD2\xA2\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\x04\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x41\x4E\xFF\xFF\x00\x00", "MFP-510"},
    {0}
};

static const model_data_t HP[] =
{
    {5, "\x20\x00\x45\x00\x0C\x04\x70\xCD\x4F\x00\x7F\x49", "LaserJet 3150"},
    {5, "\x40\x80\x84\x01\xF0\x6A", "OfficeJet"},
    {5, "\xC0\x00\x00\x00\x00", "OfficeJet 500"},
    {5, "\xC0\x00\x00\x00\x00\x8B", "Fax-920"},
    {0}
};

static const model_data_t Sharp[] =
{
    {32, "\x00\xCE\xB8\x80\x80\x11\x85\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\x00\x00\x00\x00\x00\x00\x00\x00\xED\x22\xB0\x00\x00\x90\x00", "Sharp F0-10"},
    {33, "\x00\xCE\xB8\x80\x80\x11\x85\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\x00\x00\x00\x00\x00\x00\x00\x00\xED\x22\xB0\x00\x00\x90\x00\x8C", "Sharp UX-460"},
    {33, "\x00\x4E\xB8\x80\x80\x11\x84\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\x00\x00\x00\x00\x00\x00\x00\x00\xED\x22\xB0\x00\x00\x90\x00\xAD", "Sharp UX-177"},
    {33, "\x00\xCE\xB8\x00\x84\x0D\xDD\x00\x00\xDD\xDD\x00\x00\xDD\xDD\xDD\xDD\xDD\x02\x05\x28\x02\x22\x43\x29\xED\x23\x90\x00\x00\x90\x01\x00", "Sharp FO-4810"},
    {0}
};

static const model_data_t Xerox[] =
{
    {10, "\x00\x08\x2D\x43\x57\x50\x61\x75\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01\x1A\x02\x02\x10\x01\x82\x01\x30\x34", "635 Workcenter"},
    {0}
};

static const model_data_t PitneyBowes[] = 
{
    {6, "\x79\x91\xB1\xB8\x7A\xD8", "9550"},
    {0}
};

static const model_data_t Dialogic[] = 
{
    {8, "\x56\x8B\x06\x55\x00\x15\x00\x00", "VFX/40ESC"},
    {0}
};

static const model_data_t Muratec45[] =
{
    {10, "\xF4\x91\xFF\xFF\xFF\x42\x2A\xBC\x01\x57", "M4700"},
    {0}
};

/* Muratec uses unregistered Japan code "00 00 48" */
static const model_data_t Muratec48[] =
{
    {3, "\x53\x53\x61", "M620"},
    {0}
};

/*
 * Country code is the first byte, then manufacturer is the last two bytes. See T.35.  
 * Japan is x00,
 * Canada x20,
 * Papua New Guinea x86.
 * Tunisia xAD,
 * UK xB4,
 * USA xB5,
 */

static const nsf_data_t known_nsf[] =
{
    {"\x00\x00\x00", "Unknown Japanese", TRUE},
    {"\x00\x00\x01", "Anjitsu", FALSE},
    {"\x00\x00\x02", "Nippon Telephone", FALSE},
    {"\x00\x00\x05", "Mitsuba Electric", FALSE},
    {"\x00\x00\x06", "Master Net", FALSE},
    {"\x00\x00\x09", "Xerox/Toshiba", TRUE, Xerox},
    {"\x00\x00\x0A", "Kokusai", FALSE},
    {"\x00\x00\x0D", "Logic System International", FALSE},
    {"\x00\x00\x0E", "Panasonic", FALSE, Panasonic0E},
    {"\x00\x00\x11", "Canon", FALSE, Canon},
    {"\x00\x00\x15", "Toyotsushen Machinery", FALSE},
    {"\x00\x00\x16", "System House Mind", FALSE},
    {"\x00\x00\x19", "Xerox", TRUE},
    {"\x00\x00\x1D", "Hitachi Software", FALSE},
    {"\x00\x00\x21", "Oki Electric/Lanier", TRUE},
    {"\x00\x00\x25", "Ricoh", TRUE, Ricoh},
    {"\x00\x00\x26", "Konica", FALSE},
    {"\x00\x00\x29", "Japan Wireless", FALSE},
    {"\x00\x00\x2D", "Sony", FALSE},
    {"\x00\x00\x31", "Sharp/Olivetti", FALSE, Sharp},
    {"\x00\x00\x35", "Kogyu", FALSE},
    {"\x00\x00\x36", "Japan Telecom", FALSE},
    {"\x00\x00\x3D", "IBM Japan", FALSE},
    {"\x00\x00\x39", "Panasonic", FALSE},
    {"\x00\x00\x41", "Swasaki Communication", FALSE},
    {"\x00\x00\x45", "Muratec", FALSE, Muratec45},
    {"\x00\x00\x46", "Pheonix", FALSE},
    {"\x00\x00\x48", "Muratec", FALSE, Muratec48},
    {"\x00\x00\x49", "Japan Electric", FALSE},
    {"\x00\x00\x4D", "Okura Electric", FALSE},
    {"\x00\x00\x51", "Sanyo", FALSE, Sanyo},
    {"\x00\x00\x55", "Unknown Japanese", FALSE},
    {"\x00\x00\x56", "Brother", FALSE, Brother},
    {"\x00\x00\x59", "Fujitsu", FALSE},
    {"\x00\x00\x5D", "Kuoni", FALSE},
    {"\x00\x00\x61", "Casio", FALSE},
    {"\x00\x00\x65", "Tateishi Electric", FALSE},
    {"\x00\x00\x66", "Utax/Mita", TRUE},
    {"\x00\x00\x69", "Hitachi Production", FALSE},
    {"\x00\x00\x6D", "Hitachi Telecom", FALSE},
    {"\x00\x00\x71", "Tamura Electric Works", FALSE},
    {"\x00\x00\x75", "Tokyo Electric Corp.", FALSE},
    {"\x00\x00\x76", "Advance", FALSE},
    {"\x00\x00\x79", "Panasonic", FALSE, Panasonic79},
    {"\x00\x00\x7D", "Seiko", FALSE},
    {"\x00\x08\x00", "Daiko", FALSE},
    {"\x00\x10\x00", "Funai Electric", FALSE},
    {"\x00\x20\x00", "Eagle System", FALSE},
    {"\x00\x30\x00", "Nippon Business Systems", FALSE},
    {"\x00\x40\x00", "Comtron", FALSE},
    {"\x00\x48\x00", "Cosmo Consulting", FALSE},
    {"\x00\x50\x00", "Orion Electric", FALSE},
    {"\x00\x60\x00", "Nagano Nippon", FALSE},
    {"\x00\x70\x00", "Kyocera", FALSE},
    {"\x00\x80\x00", "Kanda Networks", FALSE},
    {"\x00\x88\x00", "Soft Front", FALSE},
    {"\x00\x90\x00", "Arctic", FALSE},
    {"\x00\xA0\x00", "Nakushima", FALSE},
    {"\x00\xB0\x00", "Minolta", FALSE},
    {"\x00\xC0\x00", "Tohoku Pioneer", FALSE},
    {"\x00\xD0\x00", "USC", FALSE},
    {"\x00\xE0\x00", "Hiboshi", FALSE},
    {"\x00\xF0\x00", "Sumitomo Electric", FALSE},
    {"\x20\x41\x59", "Siemens", FALSE},
    {"\x59\x59\x01", NULL, FALSE},
    {"\x86\x00\x10", "Samsung", FALSE},
    {"\x86\x00\x8C", "Samsung", FALSE, Samsung8C},
    {"\x86\x00\x98", "Samsung", FALSE},
    {"\xAD\x00\x00", "Pitney Bowes", FALSE, PitneyBowes},
    {"\xAD\x00\x0C", "Dialogic", FALSE, Dialogic},
    {"\xAD\x00\x36", "HP", FALSE, HP},
    {"\xAD\x00\x42", "FaxTalk", FALSE},
    {"\xAD\x00\x44", NULL, TRUE},
    {"\xB4\x00\xB0", "DCE", FALSE},
    {"\xB4\x00\xB1", "Hasler", FALSE},
    {"\xB4\x00\xB2", "Interquad", FALSE},
    {"\xB4\x00\xB3", "Comwave", FALSE},
    {"\xB4\x00\xB4", "Iconographic", FALSE},
    {"\xB4\x00\xB5", "Wordcraft", FALSE},
    {"\xB4\x00\xB6", "Acorn", FALSE},
    {"\xB5\x00\x01", "Picturetel", FALSE},
    {"\xB5\x00\x20", "Conexant", FALSE},
    {"\xB5\x00\x22", "Comsat", FALSE},
    {"\xB5\x00\x24", "Octel", FALSE},
    {"\xB5\x00\x26", "ROLM", FALSE},
    {"\xB5\x00\x28", "SOFNET", FALSE},
    {"\xB5\x00\x29", "TIA TR-29 Committee", FALSE},
    {"\xB5\x00\x2A", "STF Tech", FALSE},
    {"\xB5\x00\x2C", "HKB", FALSE},
    {"\xB5\x00\x2E", "Delrina", FALSE},
    {"\xB5\x00\x30", "Dialogic", FALSE},
    {"\xB5\x00\x32", "Applied Synergy", FALSE},
    {"\xB5\x00\x34", "Syncro Development", FALSE},
    {"\xB5\x00\x36", "Genoa", FALSE},
    {"\xB5\x00\x38", "Texas Instruments", FALSE},
    {"\xB5\x00\x3A", "IBM", FALSE},
    {"\xB5\x00\x3C", "ViaSat", FALSE},
    {"\xB5\x00\x3E", "Ericsson", FALSE},
    {"\xB5\x00\x42", "Bogosian", FALSE},
    {"\xB5\x00\x44", "Adobe", FALSE},
    {"\xB5\x00\x46", "Fremont Communications", FALSE},
    {"\xB5\x00\x48", "Hayes", FALSE},
    {"\xB5\x00\x4A", "Lucent", FALSE},
    {"\xB5\x00\x4C", "Data Race", FALSE},
    {"\xB5\x00\x4E", "TRW", FALSE},
    {"\xB5\x00\x52", "Audiofax", FALSE},
    {"\xB5\x00\x54", "Computer Automation", FALSE},
    {"\xB5\x00\x56", "Serca", FALSE},
    {"\xB5\x00\x58", "Octocom", FALSE},
    {"\xB5\x00\x5C", "Power Solutions", FALSE},
    {"\xB5\x00\x5A", "Digital Sound", FALSE},
    {"\xB5\x00\x5E", "Pacific Data", FALSE},
    {"\xB5\x00\x60", "Commetrex", FALSE},
    {"\xB5\x00\x62", "BrookTrout", FALSE},
    {"\xB5\x00\x64", "Gammalink", FALSE},
    {"\xB5\x00\x66", "Castelle", FALSE},
    {"\xB5\x00\x68", "Hybrid Fax", FALSE},
    {"\xB5\x00\x6A", "Omnifax", FALSE},
    {"\xB5\x00\x6C", "HP", FALSE},
    {"\xB5\x00\x6E", "Microsoft", FALSE},
    {"\xB5\x00\x72", "Speaking Devices", FALSE},
    {"\xB5\x00\x74", "Compaq", FALSE},
    {"\xB5\x00\x76", "Trust - Cryptek", FALSE},
    {"\xB5\x00\x78", "Cylink", FALSE},
    {"\xB5\x00\x7A", "Pitney Bowes", FALSE},
    {"\xB5\x00\x7C", "Digiboard", FALSE},
    {"\xB5\x00\x7E", "Codex", FALSE},
    {"\xB5\x00\x82", "Wang Labs", FALSE},
    {"\xB5\x00\x84", "Netexpress Communications", FALSE},
    {"\xB5\x00\x86", "Cable-Sat", FALSE},
    {"\xB5\x00\x88", "MFPA", FALSE},
    {"\xB5\x00\x8A", "Telogy Networks", FALSE},
    {"\xB5\x00\x8E", "Telecom Multimedia Systems", FALSE},
    {"\xB5\x00\x8C", "AT&T", FALSE},
    {"\xB5\x00\x92", "Nuera", FALSE},
    {"\xB5\x00\x94", "K56flex", FALSE},
    {"\xB5\x00\x96", "MiBridge", FALSE},
    {"\xB5\x00\x98", "Xerox", FALSE},
    {"\xB5\x00\x9A", "Fujitsu", FALSE},
    {"\xB5\x00\x9B", "Fujitsu", FALSE},
    {"\xB5\x00\x9C", "Natural Microsystems", FALSE},
    {"\xB5\x00\x9E", "CopyTele", FALSE},
    {"\xB5\x00\xA2", "Murata", FALSE},
    {"\xB5\x00\xA4", "Lanier", FALSE},
    {"\xB5\x00\xA6", "Qualcomm", FALSE},
    {"\xB5\x00\xAA", "HylaFAX", FALSE},
    {NULL}
};


#if 0
void nsf_find_station_id(int reverse_order)
{
    const char *id = NULL;
    int idSize = 0;
    const char *maxId = NULL;
    int maxIdSize = 0;
    const char *p;

    /* Trying to find the longest printable ASCII sequence */
    for (p = (const char *) nsf + T35_VENDOR_ID_LEN, *end = p + nsf.length();
         p < end;
         p++)
    {
        if (isprint(*p))
        {
            if (!idSize++)
                id = p;
            if (idSize > maxIdSize)
            {
                max_id = id;
                max_id_size = idSize;
            }
        }
        else
        {
            id = NULL;
            id_size = 0;
        }
    }
    
    /* Minimum acceptable id length */
    const int MinIdSize = 4;

    if (maxIdSize >= min_id_size)
    {
        stationId.resize(0);
        const char *p;
        int dir;

        if (reverseOrder)
        {
            p = maxId + maxIdSize - 1;
            dir = -1;
        }
        else
        {
            p = maxId;
            dir = 1;
        }
        for (int i = 0;  i < maxIdSize;  i++)
        {
            stationId.append(*p);
            p += dir;
        }
        station_id_decoded = TRUE;
    }
}
/*- End of function --------------------------------------------------------*/
#endif

int t35_decode(const uint8_t *msg, int len, const char **vendor, const char **model)
{
    int vendor_decoded;
    const nsf_data_t *p;
    const model_data_t *pp;

    vendor_decoded = FALSE;
    if (vendor)
        *vendor = NULL;
    if (model)
        *model = NULL;
    for (p = known_nsf;  p->vendor_id;  p++)
    {
        if (len >= T35_VENDOR_ID_LEN
            &&
            memcmp(p->vendor_id, &msg[0], T35_VENDOR_ID_LEN) == 0)
        {
            if (p->vendor_name  &&  vendor)
                *vendor = p->vendor_name;
            if (p->known_models  &&  model)
            {
                for (pp = p->known_models;  pp->model_id;  pp++)
                {
                    if (len == T35_VENDOR_ID_LEN + pp->model_id_size
                        &&
                        memcmp(pp->model_id, &msg[T35_VENDOR_ID_LEN], pp->model_id_size) == 0)
                    {
                        *model = pp->model_name;
                        break;
                    }
                }
            }
#if 0
            findStationId(p->inverse_station_id_order);
#endif
            vendor_decoded = TRUE;
            break;
        }
    }
#if 0
    if (!vendor_found())
        find_station_id(0);
#endif
    return vendor_decoded;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
