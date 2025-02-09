import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Card, CardHeader, CardContent, CardTitle, CardDescription } from "@/components/ui/card"
import { invoke } from "@tauri-apps/api/tauri"
import { useState } from "react"
import { useToast } from "@/hooks/use-toast"
import { useNavigate } from "react-router-dom"

export default function EnrollmentNewPage() {
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
            setErrorMessage(`Encountered an error while enrolling: ${error}`)
        }
    }

    return (
      <div className="min-h-screen bg-gray-50">
        <nav className="bg-white shadow-sm p-2 flex items-center">
          <span className="text-lg font-semibold pb-1">Mud.band</span>
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

                        <Button type="submit" className="w-full">
                            Enroll
                        </Button>
                    </form>
                </CardContent>
            </Card>
        </div>
      </div>
    )
}
