import { Button } from "@/components/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { invoke } from "@tauri-apps/api/tauri"
import { useEffect, useState } from "react"
import { useToast } from "@/hooks/use-toast"
import DashboardBandAdminCard from "./DashboardBandAdminCard"
import { Activity, Wifi, WifiOff } from "lucide-react"

export default function DashboardStatusCard() {
  const { toast } = useToast()
  const [bandName, setBandName] = useState<string>("")
  const [isTunnelRunning, setIsTunnelRunning] = useState<boolean>(false)
  const [activeConf, setActiveConf] = useState<{
    interface: {
      name: string,
      private_ip: string
    }
  }>({ interface: { name: '', private_ip: '' } })

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
            description: `BANDEC_00619: Failed to get band name: ${resp_json.msg ? resp_json.msg : 'N/A'}`
          })
        }
      })
      .catch(err => toast({
        variant: "destructive",
        title: "Error",
        description: `BANDEC_00620: Failed to get band name: ${err}`
      }))

    invoke<boolean>("mudband_ui_tunnel_is_running")
      .then(status => setIsTunnelRunning(status))
      .catch(err => toast({
        variant: "destructive",
        title: "Error",
        description: `BANDEC_00621: Failed to get tunnel status: ${err}`
      }))

    const checkActiveConf = (): Promise<boolean> => {
      return invoke<string>("mudband_ui_get_active_conf")
        .then(resp => {
          const resp_json = JSON.parse(resp) as {
            status: number,
            msg?: string,
            conf?: {
              interface: {
                name: string,
                private_ip: string
              }
            }
          }
          if (resp_json.status === 200 && resp_json.conf) {
            setActiveConf(resp_json.conf)
            return true
          }
          return false
        })
        .catch(err => {
          toast({
            variant: "destructive",
            title: "Error",
            description: `BANDEC_00744: Failed to get active configuration: ${err}`
          })
          return false
        })
    }

    const intervalId = setInterval(() => {
      checkActiveConf().then(success => {
        if (success) {
          clearInterval(intervalId)
        }
      })
    }, 5000)

    checkActiveConf()

    return () => clearInterval(intervalId)
  }, [])

  return (
    <>
      <DashboardBandAdminCard />
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Activity className="h-5 w-5" />
            Status
          </CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-gray-600">Band name: {bandName}</p>
          {activeConf.interface.name && (
            <p className="text-sm text-gray-600">Device name: {activeConf.interface.name}</p>
          )}
          {activeConf.interface.private_ip && (
            <p className="text-sm text-gray-600">Private IP: {activeConf.interface.private_ip}</p>
          )}
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
                        description: `BANDEC_00622: Failed to disconnect tunnel: ${status}`
                      })
                    }
                  })
                  .catch(err => toast({
                    variant: "destructive",
                    title: "Error",
                    description: `BANDEC_00623: Failed to disconnect tunnel: ${err}`
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
                        description: `BANDEC_00624: Failed to start the tunnel: ${status}`
                      })
                    }
                  })
                  .catch(err => toast({
                    variant: "destructive",
                    title: "Error",
                    description: `BANDEC_00625: Failed to connect tunnel: ${err}`
                  }))
              }
            }}
          >
            {isTunnelRunning ? 
              <><WifiOff className="mr-2 h-4 w-4" /> Disconnect</> : 
              <><Wifi className="mr-2 h-4 w-4" /> Connect</>
            }
          </Button>
        </CardContent>
      </Card>
    </>
  )
}