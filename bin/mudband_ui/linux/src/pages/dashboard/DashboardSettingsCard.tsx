import { Button } from "@/components/ui/button"
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card"
import {
  Dialog,
  DialogContent,
  DialogTrigger,
} from "@/components/ui/dialog"
import { Separator } from "@/components/ui/separator"
import { useNavigate } from "react-router-dom"
import EnrollmentNewDialog from "../enrollment/EnrollmentNewDialog"
import EnrollmentChangeDialog from "../enrollment/EnrollmentChangeDialog"
import React from "react"
import { invoke } from "@tauri-apps/api/tauri"
import { useToast } from "@/hooks/use-toast"

export default function DashboardSettingsCard() {
  const { toast } = useToast()
  const navigate = useNavigate()
  const [newDialogOpen, setNewDialogOpen] = React.useState(false)
  const [changeDialogOpen, setChangeDialogOpen] = React.useState(false)
  const [unenrollDialogOpen, setUnenrollDialogOpen] = React.useState(false)
  const [isTunnelRunning, setIsTunnelRunning] = React.useState(false)

  React.useEffect(() => {
    const checkTunnelStatus = async () => {
      try {
        const isRunning = await invoke('mudband_ui_tunnel_is_running')
        setIsTunnelRunning(!!isRunning)
      } catch (error) {
        toast({
          variant: "destructive",
          title: "Error",
          description: `Failed to check tunnel status: ${error}`
        })
      }
    }

    checkTunnelStatus()
  }, [])

  const getActiveBand = async () => {
    const resp = await invoke('mudband_ui_get_active_band')
    const resp_json = JSON.parse(resp as string) as {
      status: number,
      band?: {
        uuid: string,
        jwt: string
      }
    }

    if (resp_json.status !== 200 || !resp_json.band) {
      throw new Error("Failed to get active band information")
    }

    return resp_json.band
  }

  const unenrollFromServer = async (jwt: string) => {
    const response = await fetch('https://mud.band/api/band/unenroll', {
      method: 'GET',
      headers: {
        'Authorization': `${jwt}`
      }
    })

    const unenrollData = await response.json() as {
      status: number,
      msg: string
    }

    if (unenrollData.status !== 200) {
      throw new Error(unenrollData.msg)
    }
  }

  const unenrollLocally = async (bandUuid: string) => {
    const resp = await invoke('mudband_ui_unenroll', { bandUuid })
    const resp_json = JSON.parse(resp as string) as {
      status: number,
      msg: string
    }

    if (resp_json.status !== 200) {
      throw new Error(resp_json.msg)
    }
  }

  const handleUnenroll = async () => {
    try {
      const activeBand = await getActiveBand()
      await unenrollFromServer(activeBand.jwt)
      await unenrollLocally(activeBand.uuid)
      
      setUnenrollDialogOpen(false)
      toast({
        title: "Info",
        description: "Successfully unenrolled."
      })
      navigate("/")
    } catch (error) {
      toast({
        variant: "destructive",
        title: "Error",
        description: `Failed to unenroll: ${error}`
      })
    }
  }

  return (
    <Card className="w-full">
      <CardHeader>
        <CardTitle>Settings</CardTitle>
        <CardDescription>
          Manage enrollment and other settings.
          {isTunnelRunning && (
            <p className="text-yellow-600 mt-2">
              Settings are disabled while tunnel is running
            </p>
          )}
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        <div className="space-y-4">
          <h3 className="text-lg font-medium">Enrollment</h3>
          <div className="space-y-2">
            <Dialog open={newDialogOpen} onOpenChange={setNewDialogOpen}>
              <DialogTrigger asChild>
                <Button 
                  variant="outline" 
                  className="w-full justify-start"
                  disabled={isTunnelRunning}
                >
                  New enrollment
                </Button>
              </DialogTrigger>
              <DialogContent className="sm:max-w-[425px]">
                <EnrollmentNewDialog onSuccess={() => setNewDialogOpen(false)} />
              </DialogContent>
            </Dialog>
            <Dialog open={changeDialogOpen} onOpenChange={setChangeDialogOpen}>
              <DialogTrigger asChild>
                <Button 
                  variant="outline" 
                  className="w-full justify-start"
                  disabled={isTunnelRunning}
                >
                  Change enrollment
                </Button>
              </DialogTrigger>
              <DialogContent className="sm:max-w-[425px]">
                <EnrollmentChangeDialog onSuccess={() => setChangeDialogOpen(false)} />
              </DialogContent>
            </Dialog>
          </div>
        </div>
        <Separator />
        <div className="space-y-4">
          <h3 className="text-lg font-medium text-destructive">Danger Zone</h3>
          <div className="space-y-2">
            <Dialog open={unenrollDialogOpen} onOpenChange={setUnenrollDialogOpen}>
              <DialogTrigger asChild>
                <Button 
                  variant="destructive" 
                  className="w-full justify-start"
                  disabled={isTunnelRunning}
                >
                  Unenroll
                </Button>
              </DialogTrigger>
              <DialogContent className="sm:max-w-[425px]">
                <div className="space-y-4">
                  <h2 className="text-lg font-semibold">Confirm Unenrollment</h2>
                  <p>Are you sure you want to unenroll? This action cannot be undone.</p>
                  <div className="flex justify-end space-x-2">
                    <Button
                      variant="outline"
                      onClick={() => setUnenrollDialogOpen(false)}
                    >
                      Cancel
                    </Button>
                    <Button
                      variant="destructive"
                      onClick={handleUnenroll}
                    >
                      Yes, Unenroll
                    </Button>
                  </div>
                </div>
              </DialogContent>
            </Dialog>
          </div>
        </div>
      </CardContent>
    </Card>
  )
}
