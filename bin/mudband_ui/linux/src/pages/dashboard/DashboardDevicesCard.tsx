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
}

export default function DashboardDevicesCard() {
  const { toast } = useToast()
  const [peers, setPeers] = useState<Peer[]>([])

  useEffect(() => {
    const fetchDevices = async () => {
      try {
        const conf = await invoke<string>("mudband_ui_get_active_conf")
        const conf_resp = JSON.parse(conf) as { 
          status: number,
          msg?: string,
          conf: { 
            peers: Peer[] 
          } 
        }
        if (conf_resp.status === 200) {
          setPeers(conf_resp.conf.peers)
        } else {
          toast({
            variant: "destructive",
            title: "Error",
            description: `BANDEC_00632: Failed to fetch devices: ${conf_resp.msg ? conf_resp.msg : 'N/A'}`
          })
        }
      } catch (error) {
        toast({
          variant: "destructive",
          title: "Error",
          description: `BANDEC_00633: Failed to fetch devices: ${error}`
        })
      }
    }
    fetchDevices()
  }, [])

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
              </div>
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  )
}