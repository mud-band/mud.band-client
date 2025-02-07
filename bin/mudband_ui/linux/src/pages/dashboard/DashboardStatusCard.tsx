import { Button } from "@/components/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { invoke } from "@tauri-apps/api/tauri"
import { useEffect, useState } from "react"

export default function DashboardStatusCard() {
  const [bandName, setBandName] = useState<string>("")
  const [isTunnelRunning, setIsTunnelRunning] = useState<boolean>(false)

  useEffect(() => {
    invoke<string>("mudband_ui_get_band_name")
      .then(name => setBandName(name))
      .catch(err => console.error("Failed to get band name:", err))

    invoke<boolean>("mudband_ui_tunnel_is_running")
      .then(status => setIsTunnelRunning(status))
      .catch(err => console.error("Failed to get tunnel status:", err))
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
              invoke<boolean>("mudband_ui_tunnel_disconnect")
                .then(status => setIsTunnelRunning(status))
                .catch(err => console.error("Failed to disconnect tunnel:", err))
            } else {
              invoke<boolean>("mudband_ui_tunnel_connect")
                .then(status => setIsTunnelRunning(status))
                .catch(err => console.error("Failed to connect tunnel:", err))
            }
          }}
        >
          {isTunnelRunning ? "Disconnect" : "Connect"}
        </Button>
      </CardContent>
    </Card>
  )
}