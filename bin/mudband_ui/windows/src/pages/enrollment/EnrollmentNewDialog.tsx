import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { invoke } from "@tauri-apps/api/tauri"
import { useState } from "react"
import { useToast } from "@/hooks/use-toast"
import { useNavigate } from "react-router-dom"
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription, DialogFooter } from "@/components/ui/dialog"
import { open } from '@tauri-apps/api/shell'

interface EnrollmentNewDialogProps {
    onSuccess?: () => void;
}

export default function EnrollmentNewDialog({ onSuccess }: EnrollmentNewDialogProps) {
    const navigate = useNavigate()
    const { toast } = useToast()
    const [enrollmentToken, setEnrollmentToken] = useState("")
    const [deviceName, setDeviceName] = useState("")
    const [enrollmentSecret, setEnrollmentSecret] = useState("")
    const [errorMessage, setErrorMessage] = useState<string | null>(null)
    const [showSsoDialog, setShowSsoDialog] = useState(false)
    const [ssoUrl, setSsoUrl] = useState("")
    const [showSuccessDialog, setShowSuccessDialog] = useState(false)
    const [successDialogContent, setSuccessDialogContent] = useState<{
        isPublic: boolean;
    }>({ isPublic: false })

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
            setSuccessDialogContent({
                isPublic: result.band.opt_public === 1
            })
            setShowSuccessDialog(true)
        } catch (error) {
            setErrorMessage(`Encountered an error while enrolling: ${error}`)
        }
    }

    return (
        <div className="max-w-2xl w-full">
            <div className="mb-6">
                <h2 className="text-2xl font-bold">New Enrollment</h2>
                <p className="text-muted-foreground">
                    Please enter the following information to enroll.
                </p>
            </div>
            
            <div>
                {errorMessage && (
                    <div className="mb-6 p-4 text-red-700 bg-red-100 rounded-lg border border-red-200">
                        {errorMessage}
                    </div>
                )}
                
                <form className="space-y-6" onSubmit={handleSubmit}>
                    <div className="space-y-3">
                        <Label htmlFor="enrollment_token">Enrollment Token</Label>
                        <Input 
                            id="enrollment_token"
                            value={enrollmentToken}
                            onChange={(e) => setEnrollmentToken(e.target.value)}
                            className="w-full"
                            required
                        />
                    </div>
                    
                    <div className="space-y-3">
                        <Label htmlFor="device_name">Device Name</Label>
                        <Input 
                            id="device_name"
                            value={deviceName}
                            onChange={(e) => setDeviceName(e.target.value)}
                            className="w-full"
                            required
                        />
                    </div>

                    <div className="space-y-3">
                        <Label htmlFor="enrollment_secret">Enrollment Secret</Label>
                        <Input 
                            id="enrollment_secret"
                            type="text"
                            value={enrollmentSecret}
                            onChange={(e) => setEnrollmentSecret(e.target.value)}
                            className="w-full"
                            placeholder="Optional"
                        />
                    </div>

                    <Button type="submit" className="w-full">
                        Enroll
                    </Button>
                </form>
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
                if (open === false) return;
                setShowSuccessDialog(open);
            }}>
                <DialogContent>
                    <DialogHeader>
                        <DialogTitle>Enrollment successful.</DialogTitle>
                        <DialogDescription className="pt-4 text-left">
                            {successDialogContent.isPublic ? (
                                <div className="space-y-2">
                                    <p><strong>NOTE: This band is public. This means that</strong></p>
                                    <ul className="list-disc pl-5 space-y-1">
                                        <li>Your default policy is 'block'. This means that nobody can connect to your device without your permission.</li>
                                        <li>You can change the default policy to 'allow' at the WebCLI.</li>
                                        <li>You need to add an ACL rule to allow the connection.</li>
                                        <li>To control ACL, you need to open the WebCLI which found in the menu.</li>
                                        <li>For details, please visit <a href="https://mud.band/docs/public-band" className="text-blue-600 hover:underline" target="_blank" rel="noopener noreferrer">https://mud.band/docs/public-band</a></li>
                                    </ul>
                                </div>
                            ) : (
                                <div className="space-y-2">
                                    <p><strong>NOTE: This band is private. This means that</strong></p>
                                    <ul className="list-disc pl-5 space-y-1">
                                        <li>Band admin only can control ACL rules and the default policy.</li>
                                        <li>You can't control your device.</li>
                                        <li>For details, please visit <a href="https://mud.band/docs/private-band" className="text-blue-600 hover:underline" target="_blank" rel="noopener noreferrer">https://mud.band/docs/private-band</a></li>
                                    </ul>
                                </div>
                            )}
                        </DialogDescription>
                    </DialogHeader>
                    <DialogFooter>
                        <Button onClick={() => {
                            setShowSuccessDialog(false);
                            navigate("/");
                            onSuccess?.();
                        }}>
                            Okay
                        </Button>
                    </DialogFooter>
                </DialogContent>
            </Dialog>
        </div>
    )
}
