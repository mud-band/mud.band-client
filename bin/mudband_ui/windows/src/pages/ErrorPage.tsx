import { AlertCircle } from 'lucide-react'
import { useNavigate } from 'react-router-dom'

export default function ErrorPage() {
  const navigate = useNavigate();

  return (
    <div className="min-h-screen bg-background flex items-center justify-center p-4">
      <div className="max-w-md w-full p-6 rounded-lg border-destructive/20">
        <div className="flex items-center gap-3 text-destructive mb-4">
          <AlertCircle className="h-8 w-8" />
          <h1 className="text-2xl font-bold">Connection Error</h1>
        </div>
        
        <div className="space-y-4">
          <p className="text-muted-foreground">
            Failed to connect to `mudband_service.exe` process via Windows named pipe.
            Please check the following:
          </p>
          
          <ul className="list-disc ml-4 space-y-2 text-muted-foreground">
            <li>Verify that the `mudband_service` service is running</li>
            <li>Try restarting your system and try again</li>
          </ul>

          <button 
            onClick={() => navigate('/')} 
            className="w-full mt-4 bg-primary text-primary-foreground hover:bg-primary/90 px-4 py-2 rounded-md transition-colors"
          >
            Try Again
          </button>
        </div>
      </div>
    </div>
  )
}
