import { useState, useEffect } from 'react';
import { data, useNavigate } from 'react-router-dom';
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Textarea } from "@/components/ui/textarea";
import { Dialog, DialogContent, DialogTitle } from "@/components/ui/dialog";
import { useToast } from "@/hooks/use-toast"
import { invoke } from "@tauri-apps/api/tauri"
import { os } from '@tauri-apps/api';

export default function BandCreateAsGuestPage() {
  const navigate = useNavigate();
  const { toast } = useToast();
  const [bandName, setBandName] = useState('');
  const [bandDescription, setBandDescription] = useState('');
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [processingStage, setProcessingStage] = useState<'band' | 'token' | 'enrolling'>('band');

  const generateRandomString = (length: number): string => {
    const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    let result = '';
    for (let i = 0; i < length; i++) {
      result += characters.charAt(Math.floor(Math.random() * characters.length));
    }
    return result;
  };

  const getDeviceName = async () => {
    const randomString = generateRandomString(6);
    try {
      const platform = await os.platform();
      const arch = await os.arch();
      return `${platform}-${arch}-${randomString}` || 'Unknown Device';
    } catch (error) {
      console.error('Error getting device name:', error);      
      return `Device-${randomString}`;
    }
  };

  const tryEnroll = async (enrollmentToken: string) => {
    try {
      setProcessingStage('enrolling');
      const deviceName = await getDeviceName();
      const response = await invoke("mudband_ui_enroll", {
          enrollmentToken,
          deviceName,
          enrollmentSecret: undefined
      })
      const result = JSON.parse(response as string) as {
          status: number;
          sso_url?: string;
          msg?: string
      }
      if (result.status !== 200) {
          console.log('Enrollment failed:', result);
          toast({
            variant: "destructive",
            title: "Enrollment failed",
            description: result.msg || "Failed to enroll.",
          });
          return
      }
      toast({
          title: "Info",
          description: "Enrollment successful.",
      });
      navigate("/")
    } catch (error) {
      console.error('Error enrolling:', error);
      toast({
        variant: "destructive",
        title: "Error enrolling",
        description: "There was a problem enrolling. Please try again.",
      });
    }
  };

  const createEnrollmentToken = async (jwt: string) => {
    try {
      const resp = await fetch('https://www.mud.band/api/band/anonymous/enrollment/token/create', {
        method: 'GET',
        headers: {
          'Authorization': jwt,
        },
      });
      const data = await resp.json();
      if (data['status'] === 200) {
        await tryEnroll(data['token']);
      } else {
        console.log('Enrollment token error:', data);
        toast({
          variant: "destructive",
          title: "Error creating enrollment token",
          description: data['msg'] || "There was a problem creating the enrollment token. Please try again.",
        });
      }
    } catch (error) {
      console.error('Error creating enrollment token:', error);
      toast({
        variant: "destructive",
        title: "Error creating enrollment token",
        description: "There was a problem creating the enrollment token. Please try again.",
      });
    }
  };

  const saveBandAdmin = async (bandUuid: string, jwt: string) => {
    try {
      const response = await invoke("mudband_ui_save_band_admin", {
        bandUuid,
        jwt,
      });
      const result = JSON.parse(response as string) as {
        status: number;
        msg?: string
      }
      if (result.status !== 200) {
        console.log('Save band admin failed:', result);
        toast({
          variant: "destructive",
          title: "Error saving band admin", 
          description: result.msg || "There was a problem saving the band admin. Please try again.",
        });
        return
      }
      await createEnrollmentToken(jwt);
    } catch (error) {
      console.error('Error saving band admin:', error);
      toast({
        variant: "destructive",
        title: "Error saving band admin", 
        description: "There was a problem saving the band admin. Please try again.",
      });
    }
  };

  const handleSubmit = async (event: React.FormEvent) => {
    event.preventDefault();
    setIsSubmitting(true);
    setProcessingStage('band');
    
    try {
      const response = await fetch('https://www.mud.band/api/band/anonymous/create', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          name: bandName,
          description: bandDescription
        }),
      });
      
      const data = await response.json();
      
      if (data['status'] === 200) {
        setProcessingStage('token');
        
        await saveBandAdmin(data['band_uuid'], data['jwt']);
      } else {
        console.log('Band creation error:', data);
        toast({
          variant: "destructive",
          title: "Error creating band",
          description: data['msg'] || "There was a problem creating your band. Please try again.",
        });
      }      
    } catch (error) {
      console.error('Error creating band:', error);
      toast({
        variant: "destructive",
        title: "Error creating band",
        description: "There was a problem creating your band. Please try again.",
      });
    } finally {
      setIsSubmitting(false);
    }
  };

  return (
    <div className="min-h-screen bg-gray-50">
      <nav className="bg-white shadow-sm p-2 flex items-center">
        <span className="text-lg font-semibold">Mud.band</span>
      </nav>
      <div className="container mx-auto max-w-md">
        <Card className="mt-8">
          <CardHeader>
            <CardTitle className="text-center">Create Band as Guest</CardTitle>
          </CardHeader>
          <CardContent>
            <form onSubmit={handleSubmit} className="space-y-6">
              <div className="space-y-2">
                <Label htmlFor="bandName">Band Name</Label>
                <Input
                  id="bandName"
                  value={bandName}
                  onChange={(e: React.ChangeEvent<HTMLInputElement>) => setBandName(e.target.value)}
                  required
                  autoFocus
                />
              </div>
              
              <div className="space-y-2">
                <Label htmlFor="bandDescription">Band Description</Label>
                <Textarea
                  id="bandDescription"
                  value={bandDescription}
                  onChange={(e: React.ChangeEvent<HTMLTextAreaElement>) => setBandDescription(e.target.value)}
                  rows={4}
                />
              </div>
              
              <div className="text-sm text-gray-500 mt-2">
                <p>The band type is private by default. All default ACL policies are open.</p>
              </div>
                            
              <div className="flex gap-2 w-full">
                <Button 
                  type="button" 
                  variant="outline" 
                  className="flex-1"
                  onClick={() => navigate(-1)}
                  disabled={isSubmitting}
                >
                  Back
                </Button>
                <Button 
                  type="submit" 
                  className="flex-1" 
                  disabled={isSubmitting}
                >
                  {isSubmitting ? (
                    <>
                      <span className="mr-2">
                        <svg className="animate-spin h-4 w-4 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                          <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                          <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                        </svg>
                      </span>
                      Create Band
                    </>
                  ) : (
                    "Create Band"
                  )}
                </Button>
              </div>
            </form>
          </CardContent>
        </Card>
      </div>

      <Dialog open={isSubmitting} onOpenChange={(open) => {
        if (!open && !isSubmitting) setIsSubmitting(false);
      }}>
        <DialogContent className="sm:max-w-md" aria-describedby="dialog-description">
          <DialogTitle>
            {processingStage === 'band' 
              ? 'Creating Band' 
              : processingStage === 'token' 
                ? 'Creating Enrollment Token' 
                : 'Enrolling Device'}
          </DialogTitle>
          <div className="flex flex-col items-center justify-center py-6 space-y-4">
            <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary"></div>
            <p className="text-lg font-medium" id="dialog-description">
              {processingStage === 'band' 
                ? 'Creating Band' 
                : processingStage === 'token' 
                  ? 'Creating the enrollment token' 
                  : 'Enrolling'}
            </p>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  );
}