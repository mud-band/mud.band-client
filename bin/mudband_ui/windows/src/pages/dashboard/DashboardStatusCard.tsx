import { Button } from "@/components/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { invoke } from "@tauri-apps/api/tauri"
import { useEffect, useState } from "react"
import { useToast } from "@/hooks/use-toast"

export default function DashboardStatusCard() {
  const { toast } = useToast()
  const [bandName, setBandName] = useState<string>("")
  const [isTunnelRunning, setIsTunnelRunning] = useState<boolean>(false)

  useEffect(() => {
    invoke<string>("mudband_ui_get_active_band")
      .then(resp => {
        const resp_json = JSON.parse(resp) as { 
          status: number, 
          msg?: string, 
          band?: { 
            name: string,
            uuid: string,
            opt_public: number,
            description: string,
            jwt: string,
            wireguard_privkey: string
          } 
        }
        if (resp_json.status === 200 && resp_json.band) {
          setBandName(resp_json.band.name)
        } else {
          toast({
            variant: "destructive",
            title: "Error",
            description: `BANDEC_00719: Failed to get band name: ${resp_json.msg ? resp_json.msg : 'N/A'}`
          })
        }
      })
      .catch(err => toast({
        variant: "destructive",
        title: "Error",
        description: `BANDEC_00720: Failed to get band name: ${err}`
      }))

    invoke<boolean>("mudband_ui_tunnel_is_running")
      .then(status => setIsTunnelRunning(status))
      .catch(err => toast({
        variant: "destructive",
        title: "Error",
        description: `BANDEC_00721: Failed to get tunnel status: ${err}`
      }))
  }, [])

  return (
    <Card>
      <CardHeader>
        <CardTitle>Status</CardTitle>
      </CardHeader>
      <CardContent>
        <p className="text-sm text-gray-600">Band name: {bandName}</p>
        <Button 
          className="mt-4 w-full" 
          onClick={() => {
            if (isTunnelRunning) {
              invoke<number>("mudband_ui_tunnel_disconnect")
                .then(status => {
                  if (status === 200 || status == 400) {
                    setIsTunnelRunning(false)
                    toast({
                      title: "Info",
                      description: "Successfully disconnected from tunnel."
                    })
                  } else {
                    toast({
                      variant: "destructive",
                      title: "Error",
                      description: `BANDECBANDEC_00722_00622: Failed to disconnect tunnel: ${status}`
                    })
                  }
                })
                .catch(err => toast({
                  variant: "destructive",
                  title: "Error",
                  description: `BANDEC_00723: Failed to disconnect tunnel: ${err}`
                }))
            } else {
              invoke<number>("mudband_ui_tunnel_connect")
                .then(status => {
                  if (status === 200) {
                    setIsTunnelRunning(true)
                    toast({
                      title: "Info",
                      description: "Successfully connected to tunnel."
                    })
                  } else {
                    toast({
                      variant: "destructive",
                      title: "Error",
                      description: `BANDEC_00724: Failed to start the tunnel: ${status}`
                    })
                  }
                })
                .catch(err => toast({
                  variant: "destructive",
                  title: "Error",
                  description: `BANDEC_00725: Failed to connect tunnel: ${err}`
                }))
            }
          }}
        >
          {isTunnelRunning ? "Disconnect" : "Connect"}
        </Button>
      </CardContent>
    </Card>
  )
}