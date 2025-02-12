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
 * Copyright (C) Cisco Systems Inc. 2001 - 2004.  All Rights Reserved.
 *
 * 3GPP features implementation is based on 3GPP's TS26.234-v5.60,
 * and was contributed by Ximpo Group Ltd.
 *
 * Portions created by Ximpo Group Ltd. are
 * Copyright (C) Ximpo Group Ltd. 2003, 2004.  All Rights Reserved.
 *
 *  Portions created by Adnecto d.o.o. are
 *  Copyright (C) Adnecto d.o.o. 2005.  All Rights Reserved
 *
 * Contributor(s):
 *      Dave Mackie                dmackie@cisco.com
 *      Alix Marchandise-Franquet  alix@cisco.com
 *      Ximpo Group Ltd.           mp4v2@ximpo.com
 *      Danijel Kopcinovic         danijel.kopcinovic@adnecto.net
 */

#include "src/impl.h"

namespace mp4v2 { namespace impl {

///////////////////////////////////////////////////////////////////////////////

MP4AtomInfo::MP4AtomInfo(const char* name, bool mandatory, bool onlyOne)
{
    m_name = name;
    m_mandatory = mandatory;
    m_onlyOne = onlyOne;
    m_count = 0;
}

MP4Atom::MP4Atom(MP4File& file, const char* type)
    : m_File(file)
{
    SetType(type);
    m_unknownType = false;
    m_start = 0;
    m_end = 0;
    m_largesizeMode = false;
    m_size = 0;
    m_pParentAtom = NULL;
    m_depth = 0xFF;
}

MP4Atom::~MP4Atom()
{
    uint32_t i;

    for (i = 0; i < m_pProperties.Size(); i++) {
        delete m_pProperties[i];
    }
    for (i = 0; i < m_pChildAtomInfos.Size(); i++) {
        delete m_pChildAtomInfos[i];
    }
    for (i = 0; i < m_pChildAtoms.Size(); i++) {
        delete m_pChildAtoms[i];
    }
}

MP4Atom* MP4Atom::CreateAtom( MP4File &file, MP4Atom* parent, const char* type )
{
    MP4Atom* atom = factory( file, parent, type );
    ASSERT( atom );
    return atom;
}

//
// Error log helper functions
//
MP4TrackId MP4Atom::GetTrackId() {
    MP4TrackId trackId = MP4_INVALID_TRACK_ID;
    MP4Atom *atom = this;

    const uint32_t id = ATOMID("trak");
    while (atom != NULL) {
        if(id == ATOMID(atom->GetType())) {
            MP4Property* pProperty;

            if (atom->FindProperty("trak.tkhd.trackId", &pProperty)) {
                trackId = ((MP4Integer32Property*)pProperty)->GetValue();
            }
            break;
        }

        atom = atom->GetParentAtom();
    }

    return trackId;
}

std::string MP4Atom::ErrorLocation()
{
    std::string location = "Container";
    MP4Atom *atom = this;

    uint32_t id = ATOMID("trak");
    while (atom != NULL) {
        if(id == ATOMID(atom->GetType())) {
            MP4Property* pProperty;

            if (atom->FindProperty("trak.mdia.hdlr.handlerType", &pProperty)) {
                std::string trackType = ((MP4StringProperty*)pProperty)->GetValue();
                location = MP4Track::TrackTypeName(trackType);
                if (location.compare("Unknown") == 0) {
                    location = TRACK_ERROR;
                }
            }
            break;
        }

        atom = atom->GetParentAtom();
    }

    return location;
}

void MP4Atom::LogAtomError(const std::string& category, const std::string& errorMsg, MP4LogLevel level) {
    // Determine if atom belongs to a track, and retrieve track ID if available
    MP4TrackId trackID = GetTrackId();

    if (trackID == MP4_INVALID_TRACK_ID) {
        LOG_FORMATTED_MSG(level, category, ErrorLocation(), errorMsg.c_str());
    }
    else {
        LOG_FORMATTED_TRACK_MSG(level, category, ErrorLocation(), trackID, errorMsg.c_str());
    }
}

// generate a skeletal self

void MP4Atom::Generate()
{
    uint32_t i;

    // for all properties
    for (i = 0; i < m_pProperties.Size(); i++) {
        // ask it to self generate
        m_pProperties[i]->Generate();
    }

    // for all mandatory, single child atom types
    for (i = 0; i < m_pChildAtomInfos.Size(); i++) {
        if (m_pChildAtomInfos[i]->m_mandatory
                && m_pChildAtomInfos[i]->m_onlyOne) {

            // create the mandatory, single child atom
            MP4Atom* pChildAtom =
                CreateAtom(m_File, this, m_pChildAtomInfos[i]->m_name);

            AddChildAtom(pChildAtom);

            // and ask it to self generate
            pChildAtom->Generate();
        }
    }
}

MP4Atom* MP4Atom::ReadAtom(MP4File& file, MP4Atom* pParentAtom)
{
    uint8_t hdrSize = 8;
    uint8_t extendedType[16];

    uint64_t pos = file.GetPosition();

    log.verbose1f("\"%s\": pos = 0x%" PRIx64, file.GetFilename().c_str(), pos);

    uint64_t dataSize = file.ReadUInt32();

    char type[5];
    file.ReadBytes((uint8_t*)&type[0], 4);
    type[4] = '\0';

    // extended size
    const bool largesizeMode = (dataSize == 1);
    if (dataSize == 1) {
        dataSize = file.ReadUInt64();
        hdrSize += 8;
        file.Check64BitStatus(type);
    }

    // extended type
    if (ATOMID(type) == ATOMID("uuid")) {
        file.ReadBytes(extendedType, sizeof(extendedType));
        hdrSize += sizeof(extendedType);
    }

    if (dataSize == 0) {
        // extends to EOF
        dataSize = file.GetSize() - pos;
    }

    // Prevent integer underflow due to incorrect atom size read from file
    if ( dataSize < hdrSize ) {
        ostringstream oss;
        oss << "Invalid atom size, dataSize = " << dataSize << " cannot be less than hdrSize = " << static_cast<unsigned>( hdrSize );
        std::string reasonableType = IsReasonableType(type) ? type : "????";
        file.AddParsingError(pParentAtom, MALFORMED_ATOM_ERROR(reasonableType), oss.str());

        // Skip to end of the child atom as determined by header size and attempt to continue parsing ?
        file.SetPosition(pos + hdrSize);
        return NULL;
        //throw new EXCEPTION(errorMsg.c_str());
    }
    dataSize -= hdrSize;

    log.verbose1f("\"%s\": type = \"%s\" data-size = %" PRIu64 " (0x%" PRIx64 ") hdr %u",
                  file.GetFilename().c_str(), type, dataSize, dataSize, hdrSize);

    bool atomOverflow = false;
    uint64_t overflowSize = dataSize;
    if (pos + hdrSize + dataSize > pParentAtom->GetEnd()) {
        atomOverflow = true;
        log.verbose1f("\"%s\": parent %s (%" PRIu64 ") pos %" PRIu64 " hdr %d data %" PRIu64 " sum %" PRIu64,
                      file.GetFilename().c_str(), pParentAtom->GetType(),
                      pParentAtom->GetEnd(),
                      pos,
                      hdrSize,
                      dataSize,
                      pos + hdrSize + dataSize);

        // skip to end of atom
        dataSize = pParentAtom->GetEnd() - pos - hdrSize;
    }

    bool this_is_udta = ATOMID(pParentAtom->GetType()) == ATOMID("udta");

    MP4Atom* pAtom = CreateAtom(file, pParentAtom, type);

    //    pAtom->SetFile(pFile);
    pAtom->SetStart(pos);
    pAtom->SetEnd(pos + hdrSize + dataSize);
    pAtom->SetLargesizeMode(largesizeMode);
    pAtom->SetSize(dataSize);
    if (ATOMID(type) == ATOMID("uuid")) {
        pAtom->SetExtendedType(extendedType);
    }
    if (pAtom->IsUnknownType()) {
        if (pAtom->IsRootAtom() && pParentAtom != NULL) {
            file.AddParsingError(pAtom, SPECIFICATION_ERROR, "Invalid empty atom type, probable file corruption or parsing error");
        }
        else if (!IsReasonableType(pAtom->GetType()) && !this_is_udta) {
            std::string errorMsg;
            if (!IsReasonableType(pParentAtom->GetType())) {
                errorMsg = "Parent atom and child atom types are both suspect, probable file corruption or parsing error";
            }
            else {
                errorMsg = std::string("Non alphanumeric atom type in '") + pParentAtom->GetType() +
                           "', probable file corruption or parsing error";
            }
            file.AddParsingError(pAtom, MALFORMED_ATOM_ERROR(pAtom->GetReasonableType()), errorMsg);
        } else {
            log.verbose1f("\"%s\": Info: atom type %s is unknown", file.GetFilename().c_str(),
                          pAtom->GetType());
        }

        if (dataSize > 0) {
            pAtom->AddProperty(
                new MP4BytesProperty(*pAtom, "data", dataSize));
        }
    }

    pAtom->SetParentAtom(pParentAtom);

    if (atomOverflow) {
        ostringstream oss;
        std::string parent = (pParentAtom->IsRootAtom()) ? "root" : pParentAtom->GetReasonableType();
        oss << "Invalid atom size, atom extends outside parent atom '" << parent << "'. Expected = " << pParentAtom->GetEnd() - pos << ", Actual = " << hdrSize + overflowSize << ".";
        file.AddParsingError(pAtom, MALFORMED_ATOM_ERROR(pAtom->GetReasonableType()), oss.str());

        pAtom->Skip();
        return pAtom;
    }

    try {
        pAtom->Read();
    }
    catch (Exception*) {
        // delete atom and return NULL
        delete pAtom;
        pAtom = NULL;
    }

    return pAtom;
}

bool MP4Atom::IsReasonableType(const char* type)
{
    // Unwound this.  Pricy when called a lot.
    if( isalnum((unsigned char) type[0])) {
        if( isalnum((unsigned char) type[1])) {
            if( isalnum((unsigned char) type[2])) {
                if( isalnum((unsigned char) type[3]) || type[3] == ' ' ) {
                    return true;
                }
            }
        }
    }

    return false;
}

// generic read
void MP4Atom::Read()
{
    if (ATOMID(m_type) != 0 && m_size > 1000000) {
        log.verbose1f("%s: \"%s\": %s atom size %" PRIu64 " is suspect", __FUNCTION__,
                     m_File.GetFilename().c_str(), m_type, m_size);
    }

    ReadProperties();

    // read child atoms, if we expect there to be some
    if (m_pChildAtomInfos.Size() > 0) {
        ReadChildAtoms();
    }

    Skip(); // to end of atom
}

void MP4Atom::Skip()
{
    if (m_File.GetPosition() != m_end) {
        log.verbose1f("\"%s\": Skip: %" PRIu64 " bytes",
                      m_File.GetFilename().c_str(), m_end - m_File.GetPosition());
    }
    m_File.SetPosition(m_end);
}

MP4Atom* MP4Atom::FindAtom(const char* name)
{
    if (!IsMe(name)) {
        return NULL;
    }

    if (!IsRootAtom()) {
        log.verbose1f("\"%s\": FindAtom: matched %s", 
                      GetFile().GetFilename().c_str(), name);

        name = MP4NameAfterFirst(name);

        // I'm the sought after atom
        if (name == NULL) {
            return this;
        }
    }

    // else it's one of my children
    return FindChildAtom(name);
}

bool MP4Atom::FindProperty(const char *name,
                           MP4Property** ppProperty, uint32_t* pIndex)
{
    if (!IsMe(name)) {
        return false;
    }

    if (!IsRootAtom()) {
        log.verbose1f("\"%s\": FindProperty: matched %s", 
                      GetFile().GetFilename().c_str(), name);

        name = MP4NameAfterFirst(name);

        // no property name given
        if (name == NULL) {
            return false;
        }
    }

    return FindContainedProperty(name, ppProperty, pIndex);
}

bool MP4Atom::IsMe(const char* name)
{
    if (name == NULL) {
        return false;
    }

    // root atom always matches
    if (strequal(m_type, "")) {
        return true;
    }

    // check if our atom name is specified as the first component
    if (!MP4NameFirstMatches(m_type, name)) {
        return false;
    }

    return true;
}

MP4Atom* MP4Atom::FindChildAtom(const char* name)
{
    uint32_t atomIndex = 0;

    // get the index if we have one, e.g. moov.trak[2].mdia...
    (void)MP4NameFirstIndex(name, &atomIndex);

    // need to get to the index'th child atom of the right type
    for (uint32_t i = 0; i < m_pChildAtoms.Size(); i++) {
        if (MP4NameFirstMatches(m_pChildAtoms[i]->GetType(), name)) {
            if (atomIndex == 0) {
                // this is the one, ask it to match
                return m_pChildAtoms[i]->FindAtom(name);
            }
            atomIndex--;
        }
    }

    return NULL;
}

bool MP4Atom::FindContainedProperty(const char *name,
                                    MP4Property** ppProperty, uint32_t* pIndex)
{
    uint32_t numProperties = m_pProperties.Size();
    uint32_t i;
    // check all of our properties
    for (i = 0; i < numProperties; i++) {
        if (m_pProperties[i]->FindProperty(name, ppProperty, pIndex)) {
            return true;
        }
    }

    // not one of our properties,
    // presumably one of our children's properties
    // check child atoms...

    // check if we have an index, e.g. trak[2].mdia...
    uint32_t atomIndex = 0;
    (void)MP4NameFirstIndex(name, &atomIndex);

    // need to get to the index'th child atom of the right type
    for (i = 0; i < m_pChildAtoms.Size(); i++) {
        if (MP4NameFirstMatches(m_pChildAtoms[i]->GetType(), name)) {
            if (atomIndex == 0) {
                // this is the one, ask it to match
                return m_pChildAtoms[i]->FindProperty(name, ppProperty, pIndex);
            }
            atomIndex--;
        }
    }

    log.verbose1f("\"%s\": FindProperty: no match for %s", 
                  GetFile().GetFilename().c_str(), name);
    return false;
}

bool MP4Atom::ReadProperties(uint32_t startIndex, uint32_t count)
{
    uint32_t numProperties = min(count, m_pProperties.Size() - startIndex);

    // read any properties of the atom
    for (uint32_t i = startIndex; i < startIndex + numProperties; i++) {

        m_pProperties[i]->Read(m_File);

        if (m_File.GetPosition() > m_end) {
            log.verbose1f("ReadProperties: insufficient data for property: %s pos 0x%" PRIx64 " atom end 0x%" PRIx64,
                          m_pProperties[i]->GetName(),
                          m_File.GetPosition(), m_end);

            ostringstream oss;
            oss << "Invalid atom size, overrun at property '" << m_pProperties[i]->GetName() << "'";
            GetFile().AddParsingError(this, MALFORMED_ATOM_ERROR(GetReasonableType()), oss.str());

            m_File.SetPosition(m_end);

            //throw new EXCEPTION(errorMsg.c_str());
            return false;
        }

        MP4LogLevel thisVerbosity =
            (m_pProperties[i]->GetType() == TableProperty) ?
            MP4_LOG_VERBOSE2 : MP4_LOG_VERBOSE1;

        if (log.verbosity >= thisVerbosity) {
            // log.printf(thisVerbosity,"Read: ");
            m_pProperties[i]->Dump(0, true);
        }
    }

    return true;
}

void MP4Atom::ReadChildAtoms()
{
    bool this_is_udta = ATOMID(m_type) == ATOMID("udta");

    log.verbose1f("\"%s\": of %s", m_File.GetFilename().c_str(), m_type[0] ? m_type : "root");
    for (uint64_t position = m_File.GetPosition();
            position < m_end;
            position = m_File.GetPosition()) {
        // make sure that we have enough to read at least 8 bytes
        // size and type.
        if (m_end - position < 2 * sizeof(uint32_t)) {
            ostringstream oss;
            // if we're reading udta, it's okay to have 4 bytes of 0
            if (this_is_udta &&
                    m_end - position == sizeof(uint32_t)) {
                uint32_t mbz = m_File.ReadUInt32();
                if (mbz != 0) {
                    oss << "In udta atom, end value is not zero " << std::hex << mbz;
                    GetFile().AddParsingError(this, MALFORMED_ATOM_ERROR(GetReasonableType()), oss.str(), MP4_LOG_WARNING);
                }
                continue;
            }
            // otherwise, output a warning, but don't care
            oss << "Extra " << std::to_string(m_end - position) << " bytes at end of atom";
            GetFile().AddParsingError(this, MALFORMED_ATOM_ERROR(GetReasonableType()), oss.str(), MP4_LOG_WARNING);
            for (uint64_t ix = 0; ix < m_end - position; ix++) {
                (void)m_File.ReadUInt8();
            }
            continue;
        }

        MP4Atom* pChildAtom = MP4Atom::ReadAtom(m_File, this);
        if (pChildAtom == NULL) {
            continue;
        }

        AddChildAtom(pChildAtom);

        MP4AtomInfo* pChildAtomInfo = FindAtomInfo(pChildAtom->GetType());

        // If it's root atom, and child atom is unexpected, log an error,
        // otherwise log info
        if (pChildAtomInfo == NULL && IsRootAtom()) {
            std::string errorMsg = std::string("Unexpected root level atom '") + pChildAtom->GetReasonableType() + "'";
            GetFile().AddParsingError(this, SPECIFICATION_ERROR, errorMsg);
        }
        else if (pChildAtomInfo == NULL && !this_is_udta) {
            std::string errorMsg = std::string("Unexpected child atom '") + pChildAtom->GetReasonableType() + "' in '" + GetReasonableType() + "'";
            GetFile().AddParsingError(this, SPECIFICATION_ERROR, errorMsg, MP4_LOG_INFO);
        }

        // if child atoms should have just one instance
        // and this is more than one, log an error
        if (pChildAtomInfo) {
            pChildAtomInfo->m_count++;

            if (pChildAtomInfo->m_onlyOne && pChildAtomInfo->m_count > 1) {
                std::string errorMsg = std::string("Multiple instances of atom '") + pChildAtom->GetType() + "' found in parent atom '" + GetType() + "'";
                GetFile().AddParsingError(this, SPECIFICATION_ERROR, errorMsg);
            }
        }
    }

    // if mandatory child atom doesn't exist, log an error
    uint32_t numAtomInfo = m_pChildAtomInfos.Size();
    for (uint32_t i = 0; i < numAtomInfo; i++) {
        if (m_pChildAtomInfos[i]->m_mandatory
                && m_pChildAtomInfos[i]->m_count == 0) {

            std::string parent = (IsRootAtom()) ? "root" : GetType();
            std::string errorMsg = std::string("Atom '") + parent + "' missing mandatory child atom '" + m_pChildAtomInfos[i]->m_name + "'";
            GetFile().AddParsingError(this, SPECIFICATION_ERROR, errorMsg);
        }
    }

    log.verbose1f("\"%s\": finished %s", m_File.GetFilename().c_str(), m_type);
}

MP4AtomInfo* MP4Atom::FindAtomInfo(const char* name)
{
    uint32_t numAtomInfo = m_pChildAtomInfos.Size();
    for (uint32_t i = 0; i < numAtomInfo; i++) {
        if (ATOMID(m_pChildAtomInfos[i]->m_name) == ATOMID(name)) {
            return m_pChildAtomInfos[i];
        }
    }
    return NULL;
}

// generic write
void MP4Atom::Write()
{
    BeginWrite();

    WriteProperties();

    WriteChildAtoms();

    FinishWrite();
}

void MP4Atom::Rewrite()
{
    if (!m_end) {
        // This atom hasn't been written yet...
        return;
    }

    uint64_t fPos = m_File.GetPosition();
    m_File.SetPosition(GetStart());
    Write();
    m_File.SetPosition(fPos);
}

void MP4Atom::BeginWrite(bool use64)
{
    m_start = m_File.GetPosition();
    //use64 = m_File.Use64Bits();
    if (use64) {
        m_File.WriteUInt32(1);
    } else {
        m_File.WriteUInt32(0);
    }
    m_File.WriteBytes((uint8_t*)&m_type[0], 4);
    if (use64) {
        m_File.WriteUInt64(0);
    }
    if (ATOMID(m_type) == ATOMID("uuid")) {
        m_File.WriteBytes(m_extendedType, sizeof(m_extendedType));
    }
}

void MP4Atom::FinishWrite(bool use64)
{
    m_end = m_File.GetPosition();
    m_size = (m_end - m_start);

    log.verbose1f("end: type %s %" PRIu64 " %" PRIu64 " size %" PRIu64,
                       m_type,m_start, m_end, m_size);
    //use64 = m_File.Use64Bits();
    if (use64) {
        m_File.SetPosition(m_start + 8);
        m_File.WriteUInt64(m_size);
    } else {
        ASSERT(m_size <= (uint64_t)0xFFFFFFFF);
        m_File.SetPosition(m_start);
        m_File.WriteUInt32(m_size);
    }
    m_File.SetPosition(m_end);

    // adjust size to just reflect data portion of atom
    m_size -= (use64 ? 16 : 8);
    if (ATOMID(m_type) == ATOMID("uuid")) {
        m_size -= sizeof(m_extendedType);
    }
}

void MP4Atom::WriteProperties(uint32_t startIndex, uint32_t count)
{
    uint32_t numProperties = min(count, m_pProperties.Size() - startIndex);

    log.verbose1f("Write: \"%s\": type %s", m_File.GetFilename().c_str(), m_type);

    for (uint32_t i = startIndex; i < startIndex + numProperties; i++) {
        m_pProperties[i]->Write(m_File);

        MP4LogLevel thisVerbosity =
            (m_pProperties[i]->GetType() == TableProperty) ?
            MP4_LOG_VERBOSE2 : MP4_LOG_VERBOSE1;

        if (log.verbosity >= thisVerbosity) {
            log.printf(thisVerbosity,"Write: ");
            m_pProperties[i]->Dump(0, false);
        }
    }
}

void MP4Atom::WriteChildAtoms()
{
    uint32_t size = m_pChildAtoms.Size();
    for (uint32_t i = 0; i < size; i++) {
        m_pChildAtoms[i]->Write();
    }

    log.verbose1f("Write: \"%s\": finished %s", m_File.GetFilename().c_str(), m_type);
}

void MP4Atom::AddProperty(MP4Property* pProperty)
{
    ASSERT(pProperty);
    m_pProperties.Add(pProperty);
}

void MP4Atom::AddVersionAndFlags()
{
    AddProperty(new MP4Integer8Property(*this, "version"));
    AddProperty(new MP4Integer24Property(*this, "flags"));
}

void MP4Atom::AddReserved(MP4Atom& parentAtom, const char* name, uint32_t size)
{
    MP4BytesProperty* pReserved = new MP4BytesProperty(parentAtom, name, size);
    pReserved->SetReadOnly();
    AddProperty(pReserved);
}

void MP4Atom::ExpectChildAtom(const char* name, bool mandatory, bool onlyOne)
{
    m_pChildAtomInfos.Add(new MP4AtomInfo(name, mandatory, onlyOne));
}

uint8_t MP4Atom::GetVersion()
{
    if (!strequal("version", m_pProperties[0]->GetName())) {
        return 0;
    }
    return ((MP4Integer8Property*)m_pProperties[0])->GetValue();
}

void MP4Atom::SetVersion(uint8_t version)
{
    if (!strequal("version", m_pProperties[0]->GetName())) {
        return;
    }
    ((MP4Integer8Property*)m_pProperties[0])->SetValue(version);
}

uint32_t MP4Atom::GetFlags()
{
    if (!strequal("flags", m_pProperties[1]->GetName())) {
        return 0;
    }
    return ((MP4Integer24Property*)m_pProperties[1])->GetValue();
}

void MP4Atom::SetFlags(uint32_t flags)
{
    if (!strequal("flags", m_pProperties[1]->GetName())) {
        return;
    }
    ((MP4Integer24Property*)m_pProperties[1])->SetValue(flags);
}

void MP4Atom::Dump(uint8_t indent, bool dumpImplicits)
{
    if ( m_type[0] != '\0' ) {
        // create list of ancestors
        list<string> tlist;
        for( MP4Atom* atom = this; atom; atom = atom->GetParentAtom() ) {
            const char* const type = atom->GetType();
            if( type && type[0] != '\0' )
                tlist.push_front( type );
        }

        // create contextual atom-name
        string can;
        const list<string>::iterator ie = tlist.end();
        for( list<string>::iterator it = tlist.begin(); it != ie; ++it )
            can += *it + '.';
        if( can.length() )
            can.resize( can.length() - 1 );

        log.dump(indent, MP4_LOG_VERBOSE1, "\"%s\": type %s (%s)",
                 GetFile().GetFilename().c_str(),
                 m_type, can.c_str() );
    }

    uint32_t i;
    uint32_t size;

    // dump our properties
    size = m_pProperties.Size();
    for (i = 0; i < size; i++) {

        /* skip details of tables unless we're told to be verbose */
        if (m_pProperties[i]->GetType() == TableProperty
                && (log.verbosity < MP4_LOG_VERBOSE2)) {
            log.dump(indent + 1, MP4_LOG_VERBOSE1, "\"%s\": <table entries suppressed>",
                     GetFile().GetFilename().c_str() );
            continue;
        }

        m_pProperties[i]->Dump(indent + 1, dumpImplicits);
    }

    // dump our children
    size = m_pChildAtoms.Size();
    for (i = 0; i < size; i++) {
        m_pChildAtoms[i]->Dump(indent + 1, dumpImplicits);
    }
}

uint8_t MP4Atom::GetDepth()
{
    if (m_depth < 0xFF) {
        return m_depth;
    }

    MP4Atom *pAtom = this;
    m_depth = 0;

    while ((pAtom = pAtom->GetParentAtom()) != NULL) {
        m_depth++;
        ASSERT(m_depth < 255);
    }
    return m_depth;
}

bool MP4Atom::GetLargesizeMode()
{
    return m_largesizeMode;
}

void MP4Atom::SetLargesizeMode( bool mode )
{
    m_largesizeMode = mode;
}

bool
MP4Atom::descendsFrom( MP4Atom* parent, const char* type )
{
    const uint32_t id = ATOMID( type );
    for( MP4Atom* atom = parent; atom; atom = atom->GetParentAtom() ) {
        if( id == ATOMID(atom->GetType()) )
            return true;
    }
    return false;
}

// UDTA child atom types to be constructed as MP4UdtaElementAtom.
// List gleaned from QTFF 2007-09-04.
static const char* const UDTA_ELEMENTS[] = {
    "\xA9" "arg",
    "\xA9" "ark",
    "\xA9" "cok",
    "\xA9" "com",
    "\xA9" "cpy",
    "\xA9" "day",
    "\xA9" "dir",
    "\xA9" "ed1",
    "\xA9" "ed2",
    "\xA9" "ed3",
    "\xA9" "ed4",
    "\xA9" "ed5",
    "\xA9" "ed6",
    "\xA9" "ed7",
    "\xA9" "ed8",
    "\xA9" "ed9",
    "\xA9" "fmt",
    "\xA9" "inf",
    "\xA9" "isr",
    "\xA9" "lab",
    "\xA9" "lal",
    "\xA9" "mak",
    "\xA9" "nak",
    "\xA9" "nam",
    "\xA9" "pdk",
    "\xA9" "phg",
    "\xA9" "prd",
    "\xA9" "prf",
    "\xA9" "prk",
    "\xA9" "prl",
    "\xA9" "req",
    "\xA9" "snk",
    "\xA9" "snm",
    "\xA9" "src",
    "\xA9" "swf",
    "\xA9" "swk",
    "\xA9" "swr",
    "\xA9" "wrt",
    "Allf",
    "name",
    "LOOP",
    "ptv ",
    "SelO",
    "WLOC",
    NULL // must be last
};

MP4Atom*
MP4Atom::factory( MP4File &file, MP4Atom* parent, const char* type )
{
    // type may be NULL only in case of root-atom
    if( !type )
        return new MP4RootAtom(file);

    // construct atoms which are context-savvy
    if( parent ) {
        const char* const ptype = parent->GetType();

        if( descendsFrom( parent, "ilst" )) {
            if( ATOMID( ptype ) == ATOMID( "ilst" )) {
               ASSERT( ATOMID( type ) != ATOMID( "ilst" ));  // don't allow ilst to be a child of ilst
               return new MP4ItemAtom( file, type );
            }

            if( ATOMID( type ) == ATOMID( "data" ))
                return new MP4DataAtom(file);

            if( ATOMID( ptype ) == ATOMID( "----" )) {
                if( ATOMID( type ) == ATOMID( "mean" ))
                    return new MP4MeanAtom(file);
                if( ATOMID( type ) == ATOMID( "name" ))
                    return new MP4NameAtom(file);
            }
        }
        else if( ATOMID( ptype ) == ATOMID( "meta" )) {
            if( ATOMID( type ) == ATOMID( "hdlr" ))
                return new MP4ItmfHdlrAtom(file);
        }
        else if( ATOMID( ptype ) == ATOMID( "udta" )) {
            if( ATOMID( type ) == ATOMID( "hnti" ))
                return new MP4HntiAtom(file);
            if( ATOMID( type ) == ATOMID( "hinf" ))
                return new MP4HinfAtom(file);
            for( const char* const* p = UDTA_ELEMENTS; *p; p++ )
                if( strequal( type, *p ))
                    return new MP4UdtaElementAtom( file, type );
        }
    }

    // no-context construction (old-style)
    switch( (uint8_t)type[0] ) {
        case 'S':
            if( ATOMID(type) == ATOMID("SVQ3") )
                return new MP4VideoAtom( file, type );
            if( ATOMID(type) == ATOMID("SMI ") )
                return new MP4SmiAtom(file);
            break;

        case 'a':
            if( ATOMID(type) == ATOMID("avc1") )
                return new MP4Avc1Atom(file);
            if( ATOMID(type) == ATOMID("ac-3") )
                return new MP4Ac3Atom(file);
            if( ATOMID(type) == ATOMID("avcC") )
                return new MP4AvcCAtom(file);
            if( ATOMID(type) == ATOMID("alis") )
                return new MP4UrlAtom( file, type );
            if( ATOMID(type) == ATOMID("alaw") )
                return new MP4SoundAtom( file, type );
            if( ATOMID(type) == ATOMID("alac") )
                return new MP4SoundAtom( file, type );
            break;

        case 'A':
            // AVID video
            if( ATOMID(type) == ATOMID("AVdh") ||
                ATOMID(type) == ATOMID("AVdn") ||
                ATOMID(type) == ATOMID("AVdv") ||
                ATOMID(type) == ATOMID("AVd1") ) {
                    return new MP4VideoAtom(file, type);
                }
            break;

        case 'c':
            if( ATOMID(type) == ATOMID("chap") )
                return new MP4TrefTypeAtom( file, type );
            if( ATOMID(type) == ATOMID("chpl") )
                return new MP4ChplAtom(file);
            if( ATOMID(type) == ATOMID("colr") )
                return new MP4ColrAtom(file);
            break;

        case 'd':
            if( ATOMID(type) == ATOMID("d263") )
                return new MP4D263Atom(file);
            if( ATOMID(type) == ATOMID("damr") )
                return new MP4DamrAtom(file);
            if( ATOMID(type) == ATOMID("dref") )
                return new MP4DrefAtom(file);
            if( ATOMID(type) == ATOMID("dpnd") )
                return new MP4TrefTypeAtom( file, type );
            if( ATOMID(type) == ATOMID("dac3") )
                return new MP4DAc3Atom(file);
            if( ATOMID(type) == ATOMID("dec3") )
                return new MP4DEc3Atom(file);

            // DV video
            if( ATOMID(type) == ATOMID("dv5n") ||
                ATOMID(type) == ATOMID("dv5p") ||
                ATOMID(type) == ATOMID("dvc ") ||
                ATOMID(type) == ATOMID("dvcp") ||
                ATOMID(type) == ATOMID("dvpp") ||
                ATOMID(type) == ATOMID("dvhq") ||
                ATOMID(type) == ATOMID("dvhp") ||
                ATOMID(type) == ATOMID("dvl ") ||
                ATOMID(type) == ATOMID("dvlp") ||
                ATOMID(type) == ATOMID("dvsd") ||
                ATOMID(type) == ATOMID("dvhd") ||
                ATOMID(type) == ATOMID("dv25") ||
                ATOMID(type) == ATOMID("dv50") ||
                ATOMID(type) == ATOMID("dvsl") ||
                ATOMID(type) == ATOMID("dvh1") ||
                ATOMID(type) == ATOMID("dvh2") ||
                ATOMID(type) == ATOMID("dvh3") ||
                ATOMID(type) == ATOMID("dvh4") ||
                ATOMID(type) == ATOMID("dvh5") ||
                ATOMID(type) == ATOMID("dvh6") ) {
                    return new MP4VideoAtom(file, type);
                }
            break;

        case 'e':
            if( ATOMID(type) == ATOMID("ec-3") )
                return new MP4Eac3Atom(file);
            if( ATOMID(type) == ATOMID("elst") )
                return new MP4ElstAtom(file);
            if( ATOMID(type) == ATOMID("enca") )
                return new MP4EncaAtom(file);
            if( ATOMID(type) == ATOMID("encv") )
                return new MP4EncvAtom(file);
            break;

        case 'f':
            if( ATOMID(type) == ATOMID("fl32") )
                return new MP4SoundAtom( file, type );
            if( ATOMID(type) == ATOMID("fl64") )
                return new MP4SoundAtom( file, type );
            if( ATOMID(type) == ATOMID("free") )
                return new MP4FreeAtom(file);
            if( ATOMID(type) == ATOMID("ftyp") )
                return new MP4FtypAtom(file);
            if( ATOMID(type) == ATOMID("ftab") )
                return new MP4FtabAtom(file);
            break;

        case 'g':
            if( ATOMID(type) == ATOMID("gmin") )
                return new MP4GminAtom(file);
            break;

        case 'h':
            if( ATOMID(type) == ATOMID("hdlr") )
                return new MP4HdlrAtom(file);
            if( ATOMID(type) == ATOMID("hint") )
                return new MP4TrefTypeAtom( file, type );
            if( ATOMID(type) == ATOMID("h263") )
                return new MP4VideoAtom( file, type );
            if( ATOMID(type) == ATOMID("href") )
                return new MP4HrefAtom(file);
            break;

        case 'i':
            if( ATOMID(type) == ATOMID("ipir") )
                return new MP4TrefTypeAtom( file, type );
            if( ATOMID(type) == ATOMID("ima4") )
                return new MP4SoundAtom( file, type );
            if( ATOMID(type) == ATOMID("in24") )
                return new MP4SoundAtom( file, type );
            if( ATOMID(type) == ATOMID("in32") )
                return new MP4SoundAtom( file, type );
            if( ATOMID(type) == ATOMID("ipcm") )
                return new MP4SoundAtom( file, type );
            break;

        case 'j':
            if( ATOMID(type) == ATOMID("jpeg") )
                return new MP4VideoAtom(file, "jpeg");
            break;

        case 'l':
            if( ATOMID(type) == ATOMID("lpcm") )
                return new MP4SoundAtom( file, type );
            break;

        case 'm':
            if( ATOMID(type) == ATOMID("mdhd") )
                return new MP4MdhdAtom(file);
            if( ATOMID(type) == ATOMID("mvhd") )
                return new MP4MvhdAtom(file);
            if( ATOMID(type) == ATOMID("mdat") )
                return new MP4MdatAtom(file);
            if( ATOMID(type) == ATOMID("mjp2") ||
                ATOMID(type) == ATOMID("mjpa") ||   // Motion JPEG format A
                ATOMID(type) == ATOMID("mjpb") ) {  // Motion JPEG format B
                    return new MP4VideoAtom(file, type);
                }
            if( ATOMID(type) == ATOMID("mpod") )
                return new MP4TrefTypeAtom( file, type );
            if( ATOMID(type) == ATOMID("mp4a") )
                return new MP4SoundAtom( file, type );
            if( ATOMID(type) == ATOMID("mp4s") )
                return new MP4Mp4sAtom(file);
            if( ATOMID(type) == ATOMID("mp4v") )
                return new MP4Mp4vAtom(file);

            // XDCAM MPEG-2 IMX video
            if( ATOMID(type) == ATOMID("mx5n") ||
                ATOMID(type) == ATOMID("mx5p") ||
                ATOMID(type) == ATOMID("mx4n") ||
                ATOMID(type) == ATOMID("mx4p") ||
                ATOMID(type) == ATOMID("mx3n") ||
                ATOMID(type) == ATOMID("mx3p") ) {
                    return new MP4VideoAtom(file, type);
                }
            break;

        case 'n':
            if( ATOMID(type) == ATOMID("nmhd") )
                return new MP4NmhdAtom(file);
            break;

        case 'o':
            if( ATOMID(type) == ATOMID("ohdr") )
                return new MP4OhdrAtom(file);
            break;

        case 'p':
            if( ATOMID(type) == ATOMID("pasp") )
                return new MP4PaspAtom(file);
            break;

        case 'r':
            if( ATOMID(type) == ATOMID("rtp ") )
                return new MP4RtpAtom(file);
            if( ATOMID(type) == ATOMID("raw ") )
                return new MP4VideoAtom( file, type );
            break;

        case 's':
            if( ATOMID(type) == ATOMID("s263") )
                return new MP4S263Atom(file);
            if( ATOMID(type) == ATOMID("samr") )
                return new MP4AmrAtom( file, type );
            if( ATOMID(type) == ATOMID("sawb") )
                return new MP4AmrAtom( file, type );
            if( ATOMID(type) == ATOMID("sdtp") )
                return new MP4SdtpAtom(file);
            if( ATOMID(type) == ATOMID("stbl") )
                return new MP4StblAtom(file);
            if( ATOMID(type) == ATOMID("stsd") )
                return new MP4StsdAtom(file);
            if( ATOMID(type) == ATOMID("stsz") )
                return new MP4StszAtom(file);
            if( ATOMID(type) == ATOMID("stsc") )
                return new MP4StscAtom(file);
            if( ATOMID(type) == ATOMID("stz2") )
                return new MP4Stz2Atom(file);
            if( ATOMID(type) == ATOMID("stdp") )
                return new MP4StdpAtom(file);
            if( ATOMID(type) == ATOMID("sdp ") )
                return new MP4SdpAtom(file);
            if( ATOMID(type) == ATOMID("sync") )
                return new MP4TrefTypeAtom( file, type );
            if( ATOMID(type) == ATOMID("skip") )
                return new MP4FreeAtom( file, type );
            if (ATOMID(type) == ATOMID("sowt") )
                return new MP4SoundAtom( file, type );
            break;

        case 't':
            if( ATOMID(type) == ATOMID("text") )
                return new MP4TextAtom(file);
            if( ATOMID(type) == ATOMID("tx3g") )
                return new MP4Tx3gAtom(file);
            if( ATOMID(type) == ATOMID("tkhd") )
                return new MP4TkhdAtom(file);
            if( ATOMID(type) == ATOMID("tfhd") )
                return new MP4TfhdAtom(file);
            if( ATOMID(type) == ATOMID("trun") )
                return new MP4TrunAtom(file);
            if( ATOMID(type) == ATOMID("twos") )
                return new MP4SoundAtom( file, type );
            break;

        case 'u':
            if( ATOMID(type) == ATOMID("udta") )
                return new MP4UdtaAtom(file);
            if( ATOMID(type) == ATOMID("url ") )
                return new MP4UrlAtom(file);
            if( ATOMID(type) == ATOMID("urn ") )
                return new MP4UrnAtom(file);
            if( ATOMID(type) == ATOMID("ulaw") )
                return new MP4SoundAtom( file, type );
            break;

        case 'v':
            if( ATOMID(type) == ATOMID("vmhd") )
                return new MP4VmhdAtom(file);
            break;

        case 'x':
            // XDCAM MPEG-2 video
            if( ATOMID(type) == ATOMID("xd51") ||
                ATOMID(type) == ATOMID("xd54") ||
                ATOMID(type) == ATOMID("xd55") ||
                ATOMID(type) == ATOMID("xd59") ||
                ATOMID(type) == ATOMID("xd5a") ||
                ATOMID(type) == ATOMID("xd5b") ||
                ATOMID(type) == ATOMID("xd5c") ||
                ATOMID(type) == ATOMID("xd5d") ||
                ATOMID(type) == ATOMID("xd5e") ||
                ATOMID(type) == ATOMID("xd5f") ||
                ATOMID(type) == ATOMID("xdv1") ||
                ATOMID(type) == ATOMID("xdv2") ||
                ATOMID(type) == ATOMID("xdv3") ||
                ATOMID(type) == ATOMID("xdv4") ||
                ATOMID(type) == ATOMID("xdv5") ||
                ATOMID(type) == ATOMID("xdv6") ||
                ATOMID(type) == ATOMID("xdv7") ||
                ATOMID(type) == ATOMID("xdv8") ||
                ATOMID(type) == ATOMID("xdv9") ||
                ATOMID(type) == ATOMID("xdva") ||
                ATOMID(type) == ATOMID("xdvb") ||
                ATOMID(type) == ATOMID("xdvc") ||
                ATOMID(type) == ATOMID("xdvd") ||
                ATOMID(type) == ATOMID("xdve") ||
                ATOMID(type) == ATOMID("xdvf") ||
                ATOMID(type) == ATOMID("xdhd") ||
                ATOMID(type) == ATOMID("xdh2") ) {
                    return new MP4VideoAtom(file, type);
                }
                break;

        case 'y':
            if( ATOMID(type) == ATOMID("yuv2") )
                return new MP4VideoAtom( file, type );
            break;

        case '.':
            if( ATOMID(type) == ATOMID(".mp3") )
                return new MP4SoundAtom( file, type );
            break;

        default:
            break;
    }

    // default to MP4StandardAtom implementation
    return new MP4StandardAtom( file, type ); 
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace mp4v2::impl
