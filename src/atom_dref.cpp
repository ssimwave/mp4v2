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
 * Contributor(s):
 *      Dave Mackie     dmackie@cisco.com
 */

#include "src/impl.h"

namespace mp4v2 {
namespace impl {

///////////////////////////////////////////////////////////////////////////////

MP4DrefAtom::MP4DrefAtom(MP4File &file)
        : MP4Atom(file, "dref")
{
    AddVersionAndFlags();

    MP4Integer32Property* pCount =
        new MP4Integer32Property(*this, "entryCount");
    pCount->SetReadOnly();
    AddProperty(pCount);

    ExpectChildAtom("url ", Optional, Many);
    ExpectChildAtom("urn ", Optional, Many);
    ExpectChildAtom("alis", Optional, Many);
}

void MP4DrefAtom::Read()
{
    /* do the usual read */
    MP4Atom::Read();

    // check that number of children == entryCount
    MP4Integer32Property* pCount =
        (MP4Integer32Property*)m_pProperties[2];

    if (m_pChildAtoms.Size() != pCount->GetValue()) {
        std::string errormsg = std::string("Inconsistency in number of entries. Expected = ") + std::to_string(m_pChildAtoms.Size()) +
                               std::string(" Actual = ") + std::to_string(pCount->GetValue());
        GetFile().AddParsingError(this, INVALID_PROPERTY_VALUE_ERROR("dref.entryCount"), errormsg);

        /* fix it */
        pCount->SetReadOnly(false);
        pCount->SetValue(m_pChildAtoms.Size());
        pCount->SetReadOnly(true);
    }
}

///////////////////////////////////////////////////////////////////////////////

}
} // namespace mp4v2::impl
