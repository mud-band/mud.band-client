import { Button } from "@/components/ui/button"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { invoke } from "@tauri-apps/api/tauri"
import { useEffect, useState } from "react"
import { useToast } from "@/hooks/use-toast"
import { 
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogDescription,
  DialogFooter
} from "@/components/ui/dialog"

export default function DashboardAdminCard() {
  const { toast } = useToast()
  const [isAdmin, setIsAdmin] = useState<boolean>(false)
  const [jwt, setJwt] = useState<string>("")
  const [tokenUuid, setTokenUuid] = useState<string>("")
  const [isDialogOpen, setIsDialogOpen] = useState<boolean>(false)
  const [isFetching, setIsFetching] = useState<boolean>(false)

  useEffect(() => {
    invoke<string>("mudband_ui_get_band_admin")
      .then(resp => {
        const resp_json = JSON.parse(resp) as {
          status: number,
          msg?: string,
          band_admin?: {
            band_uuid: string,
            jwt: string
          }
        }
        if (resp_json.status === 200 && resp_json.band_admin && resp_json.band_admin.jwt) {
          setIsAdmin(true)
          setJwt(resp_json.band_admin.jwt)
        }
      })
      .catch(err => toast({
        variant: "destructive",
        title: "Error",
        description: `BANDEC_00827: Failed to get band admin: ${err}`
      }))
  }, [])

  const getEnrollmentToken = async () => {
    setIsFetching(true);
    try {
      const response = await fetch('https://www.mud.band/api/band/anonymous/enrollment/token', {
        method: 'GET',
        headers: {
          'Authorization': jwt
        }
      });
      
      const data = await response.json();
      
      if (data.status === 200 && data['token_uuid']) {
        setTokenUuid(data.token_uuid);
        setIsDialogOpen(true);
      } else {
        toast({
          variant: "destructive",
          title: "Error",
          description: `Failed to get enrollment token: ${data.msg || 'Unknown error'}`
        });
      }
    } catch (error) {
      toast({
        variant: "destructive",
        title: "Error",
        description: `Failed to get enrollment token: ${error}`
      });
    } finally {
      setIsFetching(false);
    }
  };

  if (!isAdmin) return null

  return (
    <Card className="mb-4">
      <CardHeader>
        <CardTitle>Admin Controls</CardTitle>
      </CardHeader>
      <CardContent>
        <p className="text-sm text-gray-600 mb-4">You have administrator privileges for this band.</p>
        <div className="space-y-2">
          <Button 
            className="w-full flex items-center justify-center"
            variant="outline"
            onClick={getEnrollmentToken}
            disabled={isFetching}
          >
            <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 mr-2" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 7a2 2 0 012 2m4 0a6 6 0 01-7.743 5.743L11 17H9v2H7v2H4a1 1 0 01-1-1v-2.586a1 1 0 01.293-.707l5.964-5.964A6 6 0 1121 9z" />
            </svg>
            {isFetching ? "Fetching..." : "Get enrollment token"}
          </Button>
        </div>
      </CardContent>

      <Dialog open={isDialogOpen} onOpenChange={setIsDialogOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Enrollment Token</DialogTitle>
            <DialogDescription>
              Use this token to invite others to join your band:
            </DialogDescription>
          </DialogHeader>
          <div className="bg-gray-100 p-3 rounded-md font-mono text-center break-all">
            {tokenUuid}
          </div>
          <DialogFooter>
            <Button 
              onClick={() => {
                navigator.clipboard.writeText(tokenUuid);
                toast({
                  title: "Copied!",
                  description: "Token copied to clipboard"
                });
              }}
            >
              Copy to Clipboard
            </Button>
            <Button variant="outline" onClick={() => setIsDialogOpen(false)}>
              Close
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </Card>
  )
} 