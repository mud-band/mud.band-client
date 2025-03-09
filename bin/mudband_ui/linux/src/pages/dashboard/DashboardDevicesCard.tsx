import { useEffect, useState } from "react"
import { invoke } from "@tauri-apps/api/tauri"
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/ui/card"
import { useToast } from "@/hooks/use-toast"

interface Peer {
  name: string
  private_ip: string
  private_mask: string
  nat_type: number
  device_addresses: {
    address: string
    port: number
    type: string
  }[]
  endpoint_t_heartbeated?: number
}

export default function DashboardDevicesCard() {
  const { toast } = useToast()
  const [peers, setPeers] = useState<Peer[]>([])

  useEffect(() => {
    const fetchData = async () => {
      try {
        const [confData, snapshotData] = await Promise.all([
          invoke<string>("mudband_ui_get_active_conf"),
          invoke<string>("mudband_ui_get_status_snapshot")
        ]);

        console.log(confData)
        console.log(snapshotData)

        const confResp = JSON.parse(confData) as { 
          status: number,
          msg?: string,
          conf: { 
            peers: Peer[] 
          } 
        };

        const snapshotResp = JSON.parse(snapshotData) as {
          status: number,
          msg?: string,
          status_snapshot?: {
            peers: {
              endpoint_port: number
              iface_addr: string
              endpoint_ip: string
              endpoint_t_heartbeated: number
            }[]
            stats: {}
            status: {
              mfa_authentication_required: boolean
            }
          }
        };

        if (confResp.status === 200) {
          const enhancedPeers = confResp.conf.peers.map(peer => {
            if (snapshotResp.status == 200) {
              const matchingPeer = snapshotResp.status_snapshot?.peers.find(
                statusPeer => statusPeer.iface_addr === peer.private_ip
              );
            
              if (matchingPeer) {
                return {
                  ...peer,
                  endpoint_t_heartbeated: matchingPeer.endpoint_t_heartbeated
                };
              }
            }
            return peer;
          });
          
          setPeers(enhancedPeers);
        } else {
          toast({
            variant: "destructive",
            title: "Error",
            description: `BANDEC_00632: Failed to fetch devices: ${confResp.msg ? confResp.msg : 'N/A'}`
          });
        }
      } catch (error) {
        toast({
          variant: "destructive",
          title: "Error",
          description: `BANDEC_00633: Failed to fetch devices: ${error}`
        });
      }
    };
    
    fetchData();
  }, []);

  const formatTimestamp = (timestamp?: number) => {
    if (!timestamp) {
      return "Unknown";
    }
    
    const now = Math.floor(Date.now() / 1000);
    const diff = now - timestamp;
    
    if (diff < 60) {
      return "just now";
    }
    if (diff < 3600) {
      const minutes = Math.floor(diff / 60);
      return `${minutes} ${minutes === 1 ? 'minute' : 'minutes'} ago`;
    }
    if (diff < 86400) {
      const hours = Math.floor(diff / 3600);
      return `${hours} ${hours === 1 ? 'hour' : 'hours'} ago`;
    }
    if (diff < 604800) {
      const days = Math.floor(diff / 86400);
      return `${days} ${days === 1 ? 'day' : 'days'} ago`;
    }
    if (diff < 2592000) {
      const weeks = Math.floor(diff / 604800);
      return `${weeks} ${weeks === 1 ? 'week' : 'weeks'} ago`;
    }
    const months = Math.floor(diff / 2592000);
    return `${months} ${months === 1 ? 'month' : 'months'} ago`;
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle>Devices</CardTitle>
      </CardHeader>
      <CardContent>
        <p className="text-sm text-muted-foreground mb-2">List of peer devices.</p>
        
        <div className="space-y-2">
          {peers.map((peer, index) => (
            <div
              key={index}
              className="flex items-center justify-between p-2 border rounded-lg"
            >
              <span>{peer.name}</span>
              <div className="text-right">
                <div className="text-sm text-muted-foreground">IP: {peer.private_ip}</div>
                <div className="text-xs text-muted-foreground">NAT Type: {peer.nat_type}</div>
                <div className="text-xs text-muted-foreground">
                  Last Heartbeat: {formatTimestamp(peer.endpoint_t_heartbeated)}
                </div>
              </div>
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  )
}