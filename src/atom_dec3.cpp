/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MPEG4IP.
 *
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2001.  All Rights Reserved.
 * 
 * See ETSI TS 102 366 V1.2.1 Annex F for how to put E-ac3 in MP4.
 *
 * Contributor(s):
 *      Jesse Auerbach      jauerbach@imax.com
 */

#include "src/impl.h"

namespace mp4v2 {
namespace impl {

///////////////////////////////////////////////////////////////////////////////


MP4DEc3Atom::MP4DEc3Atom(MP4File &file)
        : MP4Atom(file, "dec3")
{
    AddProperty( new MP4BitfieldProperty(*this, "data_rate", 13)); /* 0 */
    AddProperty( new MP4BitfieldProperty(*this, "num_ind_sub", 3)); /* 1 */
    AddProperty( new MP4BitfieldProperty(*this, "fscod", 2)); /* 2 */
    AddProperty( new MP4BitfieldProperty(*this, "bsid", 5)); /* 3 */
    AddProperty( new MP4BitfieldProperty(*this, "bsmod", 5)); /* 4 */
    AddProperty( new MP4BitfieldProperty(*this, "acmod", 3)); /* 5 */
    AddProperty( new MP4BitfieldProperty(*this, "lfeon", 1)); /* 6 */
    AddProperty( new MP4BitfieldProperty(*this, "reserved", 3)); /* 7 */
    AddProperty( new MP4BitfieldProperty(*this, "num_dep_sub", 4)); /* 8 */
    m_pProperties[7]->SetReadOnly(true);
}

void MP4DEc3Atom::Generate()
{
    MP4Atom::Generate();
 
    ((MP4BitfieldProperty*)m_pProperties[8])->SetValue(0);
    AddProperty( new MP4BitfieldProperty(*this, "reserved", 1)); /* 9 */
    m_pProperties[9]->SetReadOnly(true);

}

void MP4DEc3Atom::Read()
{
    MP4Atom::Read();

    int count = ((MP4BitfieldProperty*)m_pProperties[8])->GetValue();
    if (count > 0) {
        AddProperty( new MP4BitfieldProperty(*this, "chan_loc", 9)); /* 9 */
        ReadProperties(9); // continue
    }
}

void MP4DEc3Atom::Dump(uint8_t indent, bool dumpImplicits)
{

    MP4BitfieldProperty* brcProp = ((MP4BitfieldProperty*)m_pProperties[0]);
    MP4BitfieldProperty* fscodProp = ((MP4BitfieldProperty*)m_pProperties[2]);
    MP4BitfieldProperty* bsidProp = ((MP4BitfieldProperty*)m_pProperties[3]);
    MP4BitfieldProperty* bsmodProp = ((MP4BitfieldProperty*)m_pProperties[4]);
    MP4BitfieldProperty* acmodProp = ((MP4BitfieldProperty*)m_pProperties[5]);
    MP4BitfieldProperty* lfeonProp = ((MP4BitfieldProperty*)m_pProperties[6]);
    
    log.dump(indent++, MP4_LOG_VERBOSE2, "\"%s\": type = dec3",
             GetFile().GetFilename().c_str() );

    if (fscodProp) { 
        uint64_t fscod = 0xFF;
        const char* fscodString; 
        const char* fscods[] = {
            "48", "44.1", "32", "Reserved",
        };

        fscod = fscodProp->GetValue();

        if (fscod < (sizeof(fscods) / sizeof(fscods[0]))) {
            fscodString = fscods[fscod];
        } else {
            fscodString = "Invalid value";
        }

        uint8_t hexWidth = fscodProp->GetNumBits() / 4;
        if (hexWidth == 0 || (fscodProp->GetNumBits() % 4)) {
            hexWidth++;
        }

        log.dump(indent, MP4_LOG_VERBOSE2, "\"%s\": fscod = %" PRIu64 " (0x%0*" PRIx64 ") <%u bits> [%s kHz]",
                 GetFile().GetFilename().c_str(),
                 fscod, (int)hexWidth, fscod, fscodProp->GetNumBits(), fscodString);
    }
    if (bsidProp)  bsidProp->Dump(indent, dumpImplicits);

    if (bsmodProp) { 
        uint64_t bsmod = 0xFF;
        const char* bsmodString; 
        const char* bsmods[] = {
            "Main audio service: complete main (CM)",
            "Main audio srrvice: music and effects (ME)",
            "Associated service: visually impaired (VI)",
            "Associated service: hearing impaired (HI)",
            "Associated service: dialogue (D)",
            "Associated service: commentary (C)",
            "Associated service: emergency (E)",
            "Associated service: voice over (VO) or Main audio service: karaoke",
        };

        bsmod = bsmodProp->GetValue();

        if (bsmod < (sizeof(bsmods) / sizeof(bsmods[0]))) {
            bsmodString = bsmods[bsmod];
        } else {
            bsmodString = "Invalid value";
        }

        uint8_t hexWidth = bsmodProp->GetNumBits() / 4;
        if (hexWidth == 0 || (bsmodProp->GetNumBits() % 4)) {
            hexWidth++;
        }

        log.dump(indent, MP4_LOG_VERBOSE2,
                "\"%s\": bsmod = %" PRIu64 " (0x%0*" PRIx64 ") <%u bits> [%s]",
                 GetFile().GetFilename().c_str(),
                 bsmod, (int)hexWidth, bsmod, bsmodProp->GetNumBits(), bsmodString);
    }
    
    if (acmodProp) { 
        uint64_t acmod = 0xFF;
        const char* acmodString; 

        const char* acmods[] = {
            "1 + 1 (Ch1, Ch2)",
            "1/0 (C)",
            "2/0 (L, R)",
            "3/0 (L, C, R)",
            "2/1 (L, R, S)",
            "3/1 (L, C, R, S)",
            "2/2 (L, R, SL, SR)",
            "3/2 (L, C, R, SL, SR)",
        };

        acmod = acmodProp->GetValue();

        if (acmod < (sizeof(acmods) / sizeof(acmods[0]))) {
            acmodString = acmods[acmod];
        } else {
            acmodString = "Invalid value";
        }

        uint8_t hexWidth = acmodProp->GetNumBits() / 4;
        if (hexWidth == 0 || (acmodProp->GetNumBits() % 4)) {
            hexWidth++;
        }

        log.dump(indent, MP4_LOG_VERBOSE2,
                 "\"%s\": acmod = %" PRIu64 " (0x%0*" PRIx64 ") <%u bits> [%s]",
                 GetFile().GetFilename().c_str(),
                 acmod, (int)hexWidth, acmod, acmodProp->GetNumBits(), acmodString);
    }

    if (lfeonProp) {
        uint64_t lfeon = lfeonProp->GetValue();
        uint8_t hexWidth = lfeonProp->GetNumBits() / 4;
        
        if (hexWidth == 0 || (lfeonProp->GetNumBits() % 4)) {
            hexWidth++;
        }
        
        log.dump(indent, MP4_LOG_VERBOSE2,
                "\"%s\": lfeon = %" PRIu64 " (0x%0*" PRIx64 ") <%u bits> [%s]",
                 GetFile().GetFilename().c_str(), lfeon, (int)hexWidth, lfeon, 
                 lfeonProp->GetNumBits(), lfeon ? "ENABLED" : "DISABLED"); 
    }
    
    if (brcProp) {
        uint32_t bit_rate = brcProp->GetValue();

        uint8_t hexWidth = brcProp->GetNumBits() / 4;
        if (hexWidth == 0 || (brcProp->GetNumBits() % 4)) {
            hexWidth++;
        }
        
        log.dump(indent, MP4_LOG_VERBOSE2,
                 "\"%s\": <%u bits> [%" PRIu32 " kbit/s]",
                 GetFile().GetFilename().c_str(),
                 brcProp->GetNumBits(), bit_rate); 
    }
}

///////////////////////////////////////////////////////////////////////////////

}
} // namespace mp4v2::impl
