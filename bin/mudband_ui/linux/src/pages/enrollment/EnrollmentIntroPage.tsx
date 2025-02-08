import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert"
import { Button } from "@/components/ui/button"
import { open } from '@tauri-apps/api/shell'
import { useNavigate } from "react-router-dom"
import { AlertTriangle } from "lucide-react"

export default function EnrollmentIntroPage() {
    const navigate = useNavigate()

    const handleSignIn = async () => {
        await open('https://mud.band/')
    }

    return (
      <div className="min-h-screen bg-gray-50">
        <nav className="bg-white shadow-sm p-2 flex items-center">
          <span className="text-lg font-semibold pb-1">Mud.band</span>
        </nav>
        <div className="container max-w-2xl p-2 space-y-6">
          <Alert variant="destructive">
              <AlertTriangle className="h-4 w-4" />
              <AlertTitle>No enrollment found.</AlertTitle>
              <AlertDescription>
                No enrollment found. Please start a new enrollment or create a new band.
              </AlertDescription>
          </Alert>

          <div className="flex gap-4">
            <Button 
                onClick={() => navigate('/enrollment/new')}
                variant="default"
            >
                Enroll
            </Button>
            <Button 
                onClick={handleSignIn}
                variant="outline"
            >
                Create
            </Button>
          </div>
        </div>
      </div>
    )
}