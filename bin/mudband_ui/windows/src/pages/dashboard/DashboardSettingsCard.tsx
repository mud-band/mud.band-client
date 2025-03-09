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
import BandCreateAsGuestDialog from "../band/BandCreateAsGuestDialog"
import React from "react"
import { invoke } from "@tauri-apps/api/tauri"
import { useToast } from "@/hooks/use-toast"
import { Users, Plus, RefreshCw, LogOut } from "lucide-react"

function Spinner() {
  return (
      <div className="animate-spin w-4 h-4 border-2 border-current border-t-transparent rounded-full">
      </div>
  );
}

export default function DashboardSettingsCard() {
  const { toast } = useToast()
  const navigate = useNavigate()
  const [newDialogOpen, setNewDialogOpen] = React.useState(false)
  const [changeDialogOpen, setChangeDialogOpen] = React.useState(false)
  const [createBandDialogOpen, setCreateBandDialogOpen] = React.useState(false)
  const [unenrollDialogOpen, setUnenrollDialogOpen] = React.useState(false)
  const [isTunnelRunning, setIsTunnelRunning] = React.useState(false)
  const [isUnenrolling, setIsUnenrolling] = React.useState(false)
  const [activeBandName, setActiveBandName] = React.useState<string>("")

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

  React.useEffect(() => {
    const fetchActiveBand = async () => {
      try {
        const resp = await invoke('mudband_ui_get_active_band')
        const resp_json = JSON.parse(resp as string) as {
          status: number,
          band?: {
            uuid: string,
            jwt: string,
            name: string
          }
        }

        if (resp_json.status === 200 && resp_json.band) {
          setActiveBandName(resp_json.band.name)
        }
      } catch (error) {
        toast({
          variant: "destructive",
          title: "Error",
          description: `Failed to get active band information: ${error}`
        })
      }
    }

    fetchActiveBand()
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

    if (unenrollData.status !== 200 &&
        unenrollData.status !== 505 &&
        unenrollData.status !== 506) {
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
    setIsUnenrolling(true)
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
    } finally {
      setIsUnenrolling(false)
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
          <h3 className="text-lg font-medium">Band</h3>
          <div className="space-y-2">
            <Dialog open={createBandDialogOpen} onOpenChange={setCreateBandDialogOpen}>
              <DialogTrigger asChild>
                <Button 
                  variant="outline" 
                  className="w-full justify-start"
                  disabled={isTunnelRunning}
                >
                  <Users className="mr-2 h-4 w-4" />
                  Create Band as Guest
                </Button>
              </DialogTrigger>
              <DialogContent className="sm:max-w-[425px]">
                <BandCreateAsGuestDialog open={createBandDialogOpen} onOpenChange={setCreateBandDialogOpen} onSuccess={() => setCreateBandDialogOpen(false)} />
              </DialogContent>
            </Dialog>
          </div>
        </div>
        <Separator className="my-4" />
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
                  <Plus className="mr-2 h-4 w-4" />
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
                  <RefreshCw className="mr-2 h-4 w-4" />
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
                  <LogOut className="mr-2 h-4 w-4" />
                  Unenroll
                </Button>
              </DialogTrigger>
              <DialogContent className="sm:max-w-[425px]">
                <div className="space-y-4">
                  <h2 className="text-lg font-semibold">Confirm Unenrollment</h2>
                  <p>Are you sure you want to unenroll from <span className="font-semibold">{activeBandName}</span>? This action cannot be undone.</p>
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
                      disabled={isUnenrolling}
                    >
                      {isUnenrolling ? (
                        <div className="flex items-center gap-2">
                          <Spinner />
                          <span>Unenrolling...</span>
                        </div>
                      ) : (
                        "Yes, Unenroll"
                      )}
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
