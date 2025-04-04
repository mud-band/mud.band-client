/*-
 * Copyright (c) 2025 Weongyo Jeong <weongyo@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

import Alamofire
import Foundation
import SwiftyJSON
import SwiftUI

struct UiDashboardUserTosAgreement: View {
    @EnvironmentObject private var mAppModel: AppModel
    @Environment(\.openURL) var openURL

    var body: some View {
        VStack {
            Text("User Agreement")
                .padding()
                .font(.largeTitle)
            Text("The following information is collected while mud.band app is running to perform P2P (Peer To Peer) connection:")
            List {
                Section {
                    Text("Public IP")
                    Text("Private / Local IP")
                    Text("NAT type")
                }
            }
            Text("No other information except for the above is logged on the mud.band server, and the data will not be shared with any third parties." + " " + "By clicking the \"I agree\" button, you agree to our Terms of Service and Privacy Policy.")
                .padding()
            HStack {
                Button {
                    mAppModel.set_user_tos_agreement(agreed: true)
                } label: {
                    Text("I agree")
                }
                Button {
                    if let url = URL(string: "https://www.mud.band/policy/tos") {
                        openURL(url)
                    }
                } label: {
                    Text("View ToS / Privacy Policy")
                }
            }
            .padding()
        }
    }
}

struct UiDashboardUserTosAgreementPreviews: PreviewProvider {
    static var previews: some View {
        UiDashboardUserTosAgreement()
    }
}
