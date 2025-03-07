import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription, DialogFooter } from "@/components/ui/dialog"
import { Card, CardHeader, CardContent, CardTitle, CardDescription } from "@/components/ui/card"
import { invoke } from "@tauri-apps/api/tauri"
import { useState } from "react"
import { useNavigate } from "react-router-dom"
import { open } from '@tauri-apps/api/shell'

function Spinner() {
    return (
        <div className="animate-spin w-4 h-4 border-2 border-current border-t-transparent rounded-full">
        </div>
    );
}

export default function EnrollmentNewPage() {
    const navigate = useNavigate()
    const [enrollmentToken, setEnrollmentToken] = useState("")
    const [deviceName, setDeviceName] = useState("")
    const [enrollmentSecret, setEnrollmentSecret] = useState("")
    const [errorMessage, setErrorMessage] = useState<string | null>(null)
    const [isLoading, setIsLoading] = useState(false)
    const [showSsoDialog, setShowSsoDialog] = useState(false)
    const [ssoUrl, setSsoUrl] = useState("")
    const [showSuccessDialog, setShowSuccessDialog] = useState(false)
    const [enrollmentResult, setEnrollmentResult] = useState<{opt_public: number} | null>(null)

    const handleOpenSsoUrl = async () => {
        try {
            await open(ssoUrl)
        } catch (error) {
            setErrorMessage(`Failed to open URL: ${error}`)
        }
    }

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault()
        setErrorMessage(null)
        setIsLoading(true)
        
        try {
            const response = await invoke("mudband_ui_enroll", {
                enrollmentToken,
                deviceName,
                enrollmentSecret: enrollmentSecret || undefined
            })
            const result = JSON.parse(response as string) as {
                status: number,
                sso_url?: string,
                msg?: string,
                band?: { 
                    name: string,
                    uuid: string,
                    opt_public: number,
                    description: string,
                    jwt: string
                } 
            }
            if (result.status !== 200) {
                if (result.status === 301 && result.sso_url) {
                    setSsoUrl(result.sso_url)
                    setShowSsoDialog(true)
                    return
                }
                setErrorMessage(result.msg || "Failed to enroll.")
                return
            }
            if (!result.band) {
                setErrorMessage("Failed to enroll.")
                return
            }
            setEnrollmentResult(result.band)
            setShowSuccessDialog(true)
        } catch (error) {
            setErrorMessage(`Encountered an error while enrolling: ${error}`)
        } finally {
            setIsLoading(false)
        }
    }

    return (
      <div className="min-h-screen bg-gray-50">
        <nav className="bg-white shadow-sm p-2 flex items-center">
          <span className="text-lg font-semibold">Mud.band</span>
        </nav>
        <div className="container mx-auto p-4 flex">
            <Card className="max-w-2xl w-full">
                <CardHeader>
                    <CardTitle>Enrollment</CardTitle>
                    <CardDescription>
                        Please enter the following information to enroll.
                    </CardDescription>
                </CardHeader>
                
                <CardContent>
                    {errorMessage && (
                        <div className="mb-4 p-4 text-red-700 bg-red-100 rounded-md">
                            {errorMessage}
                        </div>
                    )}
                    
                    <form className="space-y-6" onSubmit={handleSubmit}>
                        <div className="space-y-2">
                            <Label htmlFor="enrollment_token">Enrollment Token</Label>
                            <Input 
                                id="enrollment_token"
                                value={enrollmentToken}
                                onChange={(e) => setEnrollmentToken(e.target.value)}
                                required
                            />
                        </div>
                        
                        <div className="space-y-2">
                            <Label htmlFor="device_name">Device Name</Label>
                            <Input 
                                id="device_name"
                                value={deviceName}
                                onChange={(e) => setDeviceName(e.target.value)}
                                required
                            />
                        </div>

                        <div className="space-y-2">
                            <Label htmlFor="enrollment_secret">Enrollment Secret</Label>
                            <Input 
                                id="enrollment_secret"
                                type="text"
                                value={enrollmentSecret}
                                onChange={(e) => setEnrollmentSecret(e.target.value)}
                                placeholder="Optional"
                            />
                        </div>

                        <div className="flex space-x-4 justify-end">
                            <Button
                                type="button"
                                variant="outline"
                                onClick={() => navigate(-1)}
                                disabled={isLoading}
                            >
                                Back
                            </Button>
                            <Button 
                                type="submit"
                                disabled={isLoading}
                                className="min-w-[80px]"
                            >
                                {isLoading ? (
                                    <div className="flex items-center gap-2">
                                        <Spinner />
                                        <span>Enrolling...</span>
                                    </div>
                                ) : (
                                    "Enroll"
                                )}
                            </Button>
                        </div>
                    </form>
                </CardContent>
            </Card>
        </div>
        <Dialog open={showSsoDialog} onOpenChange={setShowSsoDialog}>
          <DialogContent>
            <DialogHeader>
              <DialogTitle>Authentication Required</DialogTitle>
              <DialogDescription>
                Additional authentication is required to complete the enrollment process. 
                Please click the button below to open the authentication page.
              </DialogDescription>
            </DialogHeader>
            <DialogFooter>
              <Button onClick={handleOpenSsoUrl}>
                Open Authentication Page
              </Button>
            </DialogFooter>
          </DialogContent>
        </Dialog>

        <Dialog open={showSuccessDialog} onOpenChange={(open) => {
            if (!open) return
        }}>
            <DialogContent>
                <DialogHeader>
                    <DialogTitle>Enrollment successful</DialogTitle>
                    <DialogDescription className="space-y-4 text-left">
                        {enrollmentResult?.opt_public === 1 ? (
                            <>
                                <p>NOTE: This band is public. This means that</p>
                                <ul className="list-disc pl-6 space-y-1">
                                    <li>Your default policy is 'block'. This means that nobody can connect to your device without your permission.</li>
                                    <li>You can change the default policy to 'allow' at the WebCLI.</li>
                                    <li>You need to add an ACL rule to allow the connection.</li>
                                    <li>To control ACL, you need to open the WebCLI which found in the menu.</li>
                                </ul>
                                <p>
                                    For details, please visit{' '}
                                    <a 
                                        href="https://mud.band/docs/public-band" 
                                        className="text-blue-600 hover:underline"
                                        onClick={(e) => {
                                            e.preventDefault()
                                            open('https://mud.band/docs/public-band')
                                        }}
                                    >
                                        https://mud.band/docs/public-band
                                    </a>
                                </p>
                            </>
                        ) : (
                            <>
                                <p>NOTE: This band is private. This means that</p>
                                <ul className="list-disc pl-6 space-y-1">
                                    <li>Band admin only can control ACL rules and the default policy.</li>
                                    <li>You can't control your device.</li>
                                </ul>
                                <p>
                                    For details, please visit{' '}
                                    <a 
                                        href="https://mud.band/docs/private-band"
                                        className="text-blue-600 hover:underline"
                                        onClick={(e) => {
                                            e.preventDefault()
                                            open('https://mud.band/docs/private-band')
                                        }}
                                    >
                                        https://mud.band/docs/private-band
                                    </a>
                                </p>
                            </>
                        )}
                    </DialogDescription>
                </DialogHeader>
                <DialogFooter>
                    <Button onClick={() => {
                        setShowSuccessDialog(false)
                        navigate('/')
                    }}>
                        Okay
                    </Button>
                </DialogFooter>
            </DialogContent>
        </Dialog>
      </div>
    )
}
