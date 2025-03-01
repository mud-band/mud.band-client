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

  const handleBandCreateAsGuest = () => {
    navigate('/band/create_as_guest')
  }

  return (
    <div className="min-h-screen bg-gray-50">
      <nav className="bg-white shadow-sm p-2 flex items-center">
        <span className="text-lg font-semibold">Mud.band</span>
      </nav>
      <div className="container max-w-2xl p-2 space-y-6">
        <Alert variant="destructive">
          <AlertTriangle className="h-4 w-4" />
          <AlertTitle>No enrollment found.</AlertTitle>
          <AlertDescription>
            No enrollment found. Please start a new enrollment or create a new band.
          </AlertDescription>
        </Alert>

        <div className="space-y-6">
          <div className="flex flex-col items-center gap-2">
            <h2 className="text-lg font-medium">Join an existing band</h2>
            <Button 
              onClick={() => navigate('/enrollment/new')}
              variant="default"
              className="w-64"
            >
              Enroll
            </Button>
          </div>
          
          <div className="flex flex-col items-center gap-2">
            <h2 className="text-lg font-medium">Create a new band</h2>
            <div className="flex flex-col gap-2 w-64">
              <Button 
                onClick={handleBandCreateAsGuest}
                variant="outline"
              >
                Create Band as Guest
              </Button>
              <Button 
                onClick={handleSignIn}
                variant="outline"
              >
                Sign in & Create Band
              </Button>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}