/*
 * Copyright (c) 2000, 2026, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 *
 *  (C) Copyright IBM Corp. 1999 All Rights Reserved.
 *  Copyright 1997 The Open Group Research Institute.  All rights reserved.
 */

package sun.security.krb5;

import sun.security.krb5.internal.*;
import sun.security.krb5.internal.crypto.KeyUsage;
import sun.security.util.*;
import java.io.IOException;

/**
 * This class encapsulates a TGS-REP that is sent from the KDC to the
 * Kerberos client.
 */
final class KrbTgsRep extends KrbKdcRep {
    private TGSRep rep;
    private Credentials creds;
    private Credentials additionalCreds;

    KrbTgsRep(byte[] ibuf, KrbTgsReq tgsReq)
        throws KrbException, IOException {
        DerValue ref = new DerValue(ibuf);
        TGSReq req = tgsReq.getMessage();
        TGSRep rep = null;
        try {
            rep = new TGSRep(ref);
        } catch (Asn1Exception e) {
            rep = null;
            KRBError err = new KRBError(ref);
            String errStr = err.getErrorString();
            String eText = null; // pick up text sent by the server (if any)
            if (errStr != null && errStr.length() > 0) {
                if (errStr.charAt(errStr.length() - 1) == 0)
                    eText = errStr.substring(0, errStr.length() - 1);
                else
                    eText = errStr;
            }
            KrbException ke;
            if (eText == null) {
                // no text sent from server
                ke = new KrbException(err.getErrorCode());
            } else {
                // override default text with server text
                ke = new KrbException(err.getErrorCode(), eText);
            }
            ke.initCause(e);
            throw ke;
        }
        byte[] enc_tgs_rep_bytes = rep.encPart.decrypt(tgsReq.tgsReqKey,
            tgsReq.usedSubkey() ? KeyUsage.KU_ENC_TGS_REP_PART_SUBKEY :
            KeyUsage.KU_ENC_TGS_REP_PART_SESSKEY);

        byte[] enc_tgs_rep_part = rep.encPart.reset(enc_tgs_rep_bytes);
        ref = new DerValue(enc_tgs_rep_part);
        EncTGSRepPart enc_part = new EncTGSRepPart(ref);
        rep.encKDCRepPart = enc_part;

        PrincipalName expectedCname = req.reqBody.cname;
        // System property to control rep.cname check:
        //
        // If true, set `expectedCname` for different cases and check that it
        // is the same as `rep.cname` (inside `check()` method). Later, `creds`
        // will be bound to this name. Otherwise, simply set `expectedCname`
        // to `rep.cname` so `check()` always succeeds.
        //
        // Default value is `true`. Set to `false` to revert to old behavior.
        if (!SecurityProperties.getBooleanSystemProp(
                "sun.security.krb5.tgs-rep.cname.check", true, null)) {
            expectedCname = rep.cname;
        } else if (req.reqBody.kdcOptions.get(KDCOptions.CNAME_IN_ADDL_TKT)) {
            // S4U2proxy
            expectedCname = tgsReq.getAdditionalCreds().getClient();
        } else {
            // S4U2Self
            if (req.pAData != null) {
                for (PAData pa : req.pAData) {
                    // We only support PA_FOR_USER for S4U2Self.
                    if (pa.getType() == Krb5.PA_FOR_USER) {
                        String[] snameStrings = rep.encKDCRepPart.sname.getNameStrings();
                        if (snameStrings.length != 2 ||
                                !snameStrings[0].equals(PrincipalName.TGS_DEFAULT_SRV_NAME)) {
                            // This is not a referral
                            PAForUserEnc p4u = new PAForUserEnc(
                                    new DerValue(pa.getValue()), null);
                            // If the KDC supports S4U2self, the expected cname should be
                            // p4u.name. Otherwise, it should be the name of the service
                            // sending the request (i.e. req.reqBody.cname). Either is OK
                            // in this method. Later, the difference will be handled in
                            // `CredentialsUtil.acquireS4U2selfCreds()`.
                            // The assignment below makes sure both values will be
                            // treated as legal in `KrbKdcRep.check()`.
                            if (p4u.name.equals(rep.cname)) {
                                expectedCname = p4u.name;
                            }
                        }
                        break;
                    }
                }
            }
        }
        check(false, req, rep, tgsReq.tgsReqKey, expectedCname);

        PrincipalName serverAlias = tgsReq.getServerAlias();
        if (serverAlias != null) {
            PrincipalName repSname = enc_part.sname;
            if (serverAlias.equals(repSname) ||
                    isReferralSname(repSname)) {
                serverAlias = null;
            }
        }

        PrincipalName clientAlias = null;
        if (expectedCname.equals(req.reqBody.cname)) {
            // Only propagate the client alias if it is not an
            // impersonation ticket (S4U2Self or S4U2Proxy).
            clientAlias = tgsReq.getClientAlias();
        }

        this.creds = new Credentials(rep.ticket,
                                expectedCname,
                                clientAlias,
                                enc_part.sname,
                                serverAlias,
                                enc_part.key,
                                enc_part.flags,
                                enc_part.authtime,
                                enc_part.starttime,
                                enc_part.endtime,
                                enc_part.renewTill,
                                enc_part.caddr
                                );
        this.rep = rep;
        this.additionalCreds = tgsReq.getAdditionalCreds();
    }

    /**
     * Return the credentials that were contained in this KRB-TGS-REP.
     */
    Credentials getCreds() {
        return creds;
    }

    sun.security.krb5.internal.ccache.Credentials setCredentials() {
        return new sun.security.krb5.internal.ccache.Credentials(
                rep, additionalCreds == null ? null : additionalCreds.ticket);
    }

    private static boolean isReferralSname(PrincipalName sname) {
        if (sname != null) {
            String[] snameStrings = sname.getNameStrings();
            if (snameStrings.length == 2 &&
                    snameStrings[0].equals(
                            PrincipalName.TGS_DEFAULT_SRV_NAME)) {
                return true;
            }
        }
        return false;
    }
}
