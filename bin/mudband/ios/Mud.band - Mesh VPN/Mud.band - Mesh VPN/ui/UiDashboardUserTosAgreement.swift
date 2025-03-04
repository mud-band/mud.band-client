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
    @Environment(\.colorScheme) var colorScheme

    var body: some View {
        ZStack {
            (colorScheme == .dark ? Color.black : Color(UIColor.systemGroupedBackground))
                .ignoresSafeArea()
            
            VStack(spacing: 16) {
                VStack(spacing: 4) {
                    Text("Mud.band")
                        .font(.system(size: 28, weight: .bold))
                    
                    Text("User Agreement")
                        .font(.system(size: 16, weight: .medium))
                        .foregroundColor(.secondary)
                }
                .padding(.top, 16)
                
                VStack(alignment: .leading, spacing: 12) {
                    Text("Information We Collect")
                        .font(.headline)
                        .padding(.bottom, 2)
                    
                    Text("The following information is collected while the app is running to perform P2P (Peer To Peer) connection:")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                        .padding(.bottom, 4)
                        .fixedSize(horizontal: false, vertical: true)
                        .multilineTextAlignment(.leading)
                    
                    VStack(alignment: .leading, spacing: 8) {
                        InfoRow(icon: "network", text: "Public IP")
                        InfoRow(icon: "house.fill", text: "Private / Local IP")
                        InfoRow(icon: "arrow.triangle.branch", text: "NAT type")
                    }
                    .padding(.leading, 4)
                    
                    Text("No other information except for the above is logged on the mud.band server, and the data will not be shared with any third parties.")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                        .padding(.top, 4)
                }
                .padding(12)
                .background(
                    RoundedRectangle(cornerRadius: 12)
                        .fill(colorScheme == .dark ? Color(UIColor.secondarySystemBackground) : Color.white)
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(Color.gray.opacity(0.2), lineWidth: 1)
                )
                
                Spacer()
                
                Text("By clicking the \"I agree\" button, you agree to our Terms of Service and Privacy Policy.")
                    .font(.footnote)
                    .multilineTextAlignment(.center)
                    .foregroundColor(.secondary)
                    .padding(.horizontal)
                
                VStack(spacing: 8) {
                    Button {
                        mAppModel.set_user_tos_agreement(agreed: true)
                    } label: {
                        Text("I Agree")
                            .font(.system(size: 16, weight: .semibold))
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 10)
                            .background(Color.blue)
                            .foregroundColor(.white)
                            .cornerRadius(10)
                    }
                    
                    Button {
                        if let url = URL(string: "https://www.mud.band/policy/tos") {
                            openURL(url)
                        }
                    } label: {
                        Text("View Terms of Service")
                            .font(.system(size: 14))
                            .foregroundColor(.blue)
                    }
                    .padding(.bottom, 4)
                }
                .padding(.horizontal, 20)
                .padding(.bottom, 16)
            }
            .padding(12)
        }
    }
}

struct InfoRow: View {
    let icon: String
    let text: String
    
    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: icon)
                .foregroundColor(.blue)
                .frame(width: 20, height: 20)
            
            Text(text)
                .font(.system(size: 14))
        }
    }
}

struct UiDashboardUserTosAgreementPreviews: PreviewProvider {
    static var previews: some View {
        UiDashboardUserTosAgreement()
    }
}
