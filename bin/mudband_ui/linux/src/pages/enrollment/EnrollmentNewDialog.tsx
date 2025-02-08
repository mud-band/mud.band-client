import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { invoke } from "@tauri-apps/api/tauri"
import { useState } from "react"
import { useToast } from "@/hooks/use-toast"
import { useNavigate } from "react-router-dom"

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

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault()
        setErrorMessage(null)
        
        try {
            const response = await invoke("mudband_ui_enroll", {
                enrollmentToken,
                deviceName,
                enrollmentSecret: enrollmentSecret || undefined
            })
            const result = JSON.parse(response as string) as { status: number; msg?: string }
            
            if (result.status !== 200) {
                setErrorMessage(result.msg || "Failed to enroll.")
                return
            }
            toast({
                title: "Info",
                description: "Enrollment successful.",
            });
            navigate("/")
            onSuccess?.();
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
        </div>
    )
}
