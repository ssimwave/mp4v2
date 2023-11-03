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
 * See ETSI TS 102 366 V1.2.1 Annex F for how to put Ac3 in MP4.
 * 
 * Contributor(s):
 *      Edward Groenendaal      egroenen@cisco.com
 */

#include "src/impl.h"

namespace mp4v2 {
namespace impl {

///////////////////////////////////////////////////////////////////////////////

MP4Ac3Atom::MP4Ac3Atom(MP4File &file)
        : MP4SoundAtom(file, "ac-3")
{
    ExpectChildAtom("dac3", Required, OnlyOne);
}

MP4Eac3Atom::MP4Eac3Atom(MP4File &file)
        : MP4SoundAtom(file, "ec-3")
{
    ExpectChildAtom("dec3", Required, OnlyOne);
}

///////////////////////////////////////////////////////////////////////////////

}
} // namespace mp4v2::impl
