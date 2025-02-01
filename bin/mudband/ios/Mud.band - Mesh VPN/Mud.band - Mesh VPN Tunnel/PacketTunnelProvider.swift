//
// Copyright (c) 2024 Weongyo Jeong <weongyo@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

import Alamofire
import Foundation
import NetworkExtension
import os
import SwiftyJSON

@objc class PacketTunnelProvider: NEPacketTunnelProvider {
    private let mPacketSerialQueue = DispatchQueue(label: "mudband tunnel packet serial queue")
    private let mUdpListenQueue = DispatchQueue(label: "mudband tunnel udp listen queue")
    private var mMfaRequired: Bool = false
    private var mMfaURL: String = ""

    override init() {
        FileManager.initMudbandTunnelGroupDirs()
        let r = mudband_tunnel_init(FileManager.TopDirURL?.path,
                                    FileManager.EnrollDirURL?.path)
        if r == -1 {
            os_log("BANDEC_00290: mudband_tunnel_init() failed.")
        }
        super.init()
    }

    struct band_conf_interface_params: Encodable {
        let listen_port: Int
        let addresses: [String]
    }

    struct band_conf_params: Encodable {
        let fetch_type: String
        let stun_mapped_addr: String
        let stun_nattype: Int
        let interface: band_conf_interface_params
    }
    
    enum band_tunnel_error: Error {
        case no_jwt_token_found
        case getifaddrs_failure
        case variable_cast_failure
        case response_parse_failure
        case no_private_ip_mask_found
        case wrong_mtu_found
        case response_status_error
        case mfa_required
    }
    
    func ipv4ToInt64(_ ipv4: String) -> Int64? {
        let components = ipv4.split(separator: ".").compactMap { UInt8($0) }
        guard components.count == 4 else {
            return nil
        }
        let result = Int64(components[0]) << 24 |
                     Int64(components[1]) << 16 |
                     Int64(components[2]) << 8  |
                     Int64(components[3])
        return result
    }
    
    func int64ToIPv4(_ value: Int64) -> String {
        let octet1 = (value >> 24) & 0xFF
        let octet2 = (value >> 16) & 0xFF
        let octet3 = (value >> 8) & 0xFF
        let octet4 = value & 0xFF

        return "\(octet1).\(octet2).\(octet3).\(octet4)"
    }
    
    func mudband_tunnel_confmgr_fetch(fetchType: String, fetchCompletionHandler: @escaping (Error?, String) -> Void) {
        var headers: HTTPHeaders = []
        if let jwt = mudband_tunnel_enroll_get_jwt() {
            headers["Authorization"] = jwt
        } else {
            mudband_tunnel_log(0, "BANDEC_00291: mudband_tunnel_enroll_get_jwt() failed.")
            fetchCompletionHandler(band_tunnel_error.no_jwt_token_found, "No JWT token found")
            return
        }
        if let etag = mudband_tunnel_confmgr_get_etag() {
            headers["If-None-Match"] = etag
        } else {
            mudband_tunnel_log(2, "No etag found.  Go forward without it.")
        }
        let maddresses = mudband_tunnel_confmgr_getifaddrs()
        if maddresses == nil {
            mudband_tunnel_log(0, "BANDEC_00292: mudband_tunnel_confmgr_getifaddrs() failed.")
            fetchCompletionHandler(band_tunnel_error.getifaddrs_failure, "getifaddrs(3) failed")
            return
        }
        if let addresses = maddresses as? [String] {
            let interface = band_conf_interface_params(listen_port: Int(mudband_tunnel_connmgr_listen_port()),
                                                       addresses: addresses)
            let parameters = band_conf_params(fetch_type: fetchType,
                                              stun_mapped_addr: mudband_tunnel_stun_client_get_mappped_addr(),
                                              stun_nattype: Int(mudband_tunnel_stun_client_get_nattype_int()),
                                              interface: interface)
            AF.request("https://www.mud.band/api/band/conf",
                       method: .post,
                       parameters: parameters,
                       encoder: JSONParameterEncoder.default,
                       headers: headers,
                       interceptor: .retryPolicy)
            .responseString { resp in
                switch resp.result {
                case .success(let resp_body):
                    if let obj = try? JSON(data: Data(resp_body.utf8)) {
                        if obj["status"].intValue == 301 {
                            fetchCompletionHandler(band_tunnel_error.mfa_required, obj["sso_url"].stringValue)
                            return
                        }
                    }
                    let etag = resp.response?.allHeaderFields["Etag"] as? String
                    let rv = mudband_tunnel_confmgr_parse_response(etag, resp_body)
                    if rv != 0 {
                        mudband_tunnel_log(0, "BANDEC_00293: mudband_tunnel_confmgr_parse_response() failed.")
                        fetchCompletionHandler(band_tunnel_error.response_parse_failure, "Response parsing error")
                        return
                    }
                    fetchCompletionHandler(nil, "Okay")
                    break
                case .failure(let error):
                    guard let statusCode = resp.response?.statusCode else {
                        mudband_tunnel_log(0, "BANDEC_00294: Failed to set the status code.")
                        fetchCompletionHandler(band_tunnel_error.response_status_error, "Response status error")
                        return
                    }
                    if statusCode != 304 {
                        mudband_tunnel_log(0, "BANDEC_00295: \(statusCode) \(error)")
                        fetchCompletionHandler(error, "\(error)")
                        return
                    }
                    mudband_tunnel_log(2, "No config changed.")
                    fetchCompletionHandler(nil, "No config changed")
                    break
                }
            }
        } else {
            mudband_tunnel_log(0, "BANDEC_00296: failed to convert NSMutableArray to Swift string array.")
            fetchCompletionHandler(band_tunnel_error.variable_cast_failure, "Cast failure")
        }
    }
    
    @objc func mudband_tunnel_confmgr_fetch_mqtt_event() {
        self.mudband_tunnel_confmgr_fetch(fetchType: "when_it_gots_a_event",
                                          fetchCompletionHandler: { (error, msg) -> Void in
            if let error = error {
                if error as! PacketTunnelProvider.band_tunnel_error == band_tunnel_error.mfa_required {
                    mudband_tunnel_log(0, "BANDEC_XXXXX: MFA required to start the tunnel. Please visit URL: \(msg)")
                    self.mMfaRequired = true
                    self.mMfaURL = msg
                    return
                }
                mudband_tunnel_log(0, "BANDEC_00297: \(error)")
                return
            }
            mudband_tunnel_log(2, "Succesfully fetched the band config.")
            mudband_tunnel_iface_need_peers_update()
        })
    }
    
    private func mudband_tunnel_ticks() {
        self.mPacketSerialQueue.asyncAfter(deadline: .now() + .milliseconds(300)) {
            mudband_tunnel_wireguard_ticks()
            self.mudband_tunnel_ticks()
        }
    }
    
    private func mudband_tunnel_udp2tun() {
        self.mUdpListenQueue.async {
            let r = mudband_tunnel_wireguard_rx_listen()
            if r == -1 {
                mudband_tunnel_log(0, "BANDEC_00298: mudband_tunnel_wireguard_rx_listen() failed.")
                return
            }
            self.mPacketSerialQueue.sync {
                let _ = mudband_tunnel_wireguard_rx_recvfrom()
            }
            self.mudband_tunnel_udp2tun()
        }
    }
    
    @objc func mudband_tunnel_tun_write(data: NSData) {
        self.packetFlow.writePackets([ Data(referencing: data) ],
                                     withProtocols: [NSNumber](repeating: AF_INET as NSNumber, count: 1))
    }
    
    private func mudband_tunnel_tun2udp() {
        self.packetFlow.readPackets { (packets: [Data], protocols: [NSNumber]) in
            self.mPacketSerialQueue.sync {
                for packet in packets {
                    mudband_tunnel_wireguard_tx(packet)
                }
            }
            self.mudband_tunnel_tun2udp()
        }
    }
    
    private func mudband_tunnel_main() {
        mudband_tunnel_wireguard_init()
        mudband_tunnel_iface_set_tunnel_provider(self)
        self.mudband_tunnel_tun2udp()
        self.mudband_tunnel_udp2tun()
        self.mudband_tunnel_ticks()
    }
    
    override func startTunnel(options: [String : NSObject]?, completionHandler: @escaping (Error?) -> Void) {
        self.reasserting = false
        self.mudband_tunnel_confmgr_fetch(fetchType: "when_it_runs_first",
                                          fetchCompletionHandler: { (error, msg) -> Void in
            if let error = error {
                if error as! PacketTunnelProvider.band_tunnel_error == band_tunnel_error.mfa_required {
                    completionHandler(error)
                    mudband_tunnel_log(0, "BANDEC_XXXXX: MFA required to start the tunnel. Please visit URL: \(msg)")
                    self.mMfaRequired = true
                    self.mMfaURL = msg
                    return
                }
                mudband_tunnel_log(0, "BANDEC_XXXXX: \(error) \(msg)")
                completionHandler(error)
                return
            }
            self.setTunnelNetworkSettings(nil) { (error: Error?) -> Void in
                if let error = error {
                    mudband_tunnel_log(0, "BANDEC_00299: \(error)")
                    completionHandler(error)
                    return
                }
                let mtu = mudband_tunnel_confmgr_get_interface_mtu()
                if mtu <= 0 {
                    mudband_tunnel_log(0, "BANDEC_00300: MTU value \(mtu) isn't good.")
                    completionHandler(band_tunnel_error.wrong_mtu_found)
                    return
                }
                guard let private_ip = mudband_tunnel_confmgr_get_interface_private_ip(),
                      let private_mask = mudband_tunnel_confmgr_get_interface_private_mask(),
                      let private_ip_int = self.ipv4ToInt64(private_ip),
                      let private_mask_int = self.ipv4ToInt64(private_mask) else {
                    mudband_tunnel_log(0, "BANDEC_00301: Wrong private IP or mask found.")
                    completionHandler(band_tunnel_error.no_private_ip_mask_found)
                    return
                }
                let private_dest = private_ip_int & private_mask_int
                let tunnelNetworkSettings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: "1.1.1.1")
                tunnelNetworkSettings.ipv4Settings = NEIPv4Settings(addresses: [ private_ip ],
                                                                    subnetMasks: [ private_mask ])
                let iroute = NEIPv4Route(destinationAddress: self.int64ToIPv4(private_dest),
                                         subnetMask: private_mask)
                tunnelNetworkSettings.ipv4Settings?.includedRoutes = [ iroute ]
                tunnelNetworkSettings.ipv4Settings?.excludedRoutes = []
                tunnelNetworkSettings.mtu = 1400
                
                self.setTunnelNetworkSettings(tunnelNetworkSettings) { (error: Error?) -> Void in
                    if let error = error {
                        mudband_tunnel_log(0, "BANDEC_00302: \(error)")
                        completionHandler(error)
                        return
                    }
                    /* Completed to setup the tunnel.  Let's loop. */
                    self.mudband_tunnel_main()
                }
            }
        })
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        // Add code here to start the process of stopping the tunnel.
        os_log("[WEONGYO] --> stopTunnel")
        completionHandler()
    }
    
    override func handleAppMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)?) {
        guard let messageString = NSString(data: messageData, encoding: String.Encoding.utf8.rawValue) else {
            mudband_tunnel_log(0, "BANDEC_00509: Failed to parse the messageData.")
            let jsonResponse = JSON([
                "status": 501,
                "msg": "Failed to parse the messageData"
            ])
            let responseData = try? jsonResponse.rawData()
            completionHandler?(responseData)
            return
        }
        if messageString == "ping" {
            if (self.mMfaRequired) {
                let jsonResponse = JSON([
                    "status": 301,
                    "sso_url": self.mMfaURL
                ])
                self.mMfaRequired = false
                self.mMfaURL = ""
                let responseData = try? jsonResponse.rawData()
                completionHandler?(responseData)
            } else {
                let jsonResponse = JSON([
                    "status": 200,
                    "msg": "Okay"
                ])
                let responseData = try? jsonResponse.rawData()
                completionHandler?(responseData)
            }
        } else {
            mudband_tunnel_log(0, "BANDEC_00510: Unknown message \(messageString)")
            let jsonResponse = JSON([
                "status": 502,
                "msg": "Unknown message \(messageString)"
            ])
            let responseData = try? jsonResponse.rawData()
            completionHandler?(responseData)
        }
    }
    
    override func sleep(completionHandler: @escaping () -> Void) {
        // Add code here to get ready to sleep.
        os_log("[WEONGYO] --> sleep")
        completionHandler()
    }
    
    override func wake() {
        // Add code here to wake up.
        os_log("[WEONGYO] --> wake")
    }
}
