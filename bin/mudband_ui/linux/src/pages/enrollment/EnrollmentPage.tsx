import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { invoke } from "@tauri-apps/api/tauri"
import { useState } from "react"
import { useToast } from "@/hooks/use-toast"
import { useNavigate } from "react-router-dom"

export default function EnrollmentPage() {
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
        } catch (error) {
            console.error("Enrollment failed:", error)
            setErrorMessage("Encountered an error while enrolling.")
        }
    }

    return (
        <div className="container mx-auto py-10">
            <div className="max-w-2xl mx-auto bg-white p-8 rounded-lg shadow-sm">
                <div className="mb-6">
                    <h1 className="text-2xl font-bold">Enrollment</h1>
                    <p className="text-gray-500 mt-1">Please enter the following information to enroll.</p>
                </div>
                
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

                    <Button type="submit" className="w-full">
                        Enroll
                    </Button>
                </form>
            </div>
        </div>
    )
}
