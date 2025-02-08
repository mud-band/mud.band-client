import { Button } from "@/components/ui/button"
import { shell } from "@tauri-apps/api"
import { invoke } from "@tauri-apps/api/tauri"

export default function TermsOfServicePage() {
  return (
    <div>
      <div className="max-w-2xl mx-auto bg-white p-8 rounded-lg">
        <h1 className="text-3xl font-bold mb-6 text-gray-900">User Agreement</h1>
        
        <p className="text-gray-700 mb-4">
          The following information is collected while mud.band app is running
          to perform P2P (Peer To Peer) connection:
        </p>
        
        <ul className="list-disc list-inside mb-4 text-gray-700 space-y-2 pl-4">
          <li>Public IP</li>
          <li>Private / Local IP</li>
          <li>NAT Type</li>
        </ul>
        
        <p className="text-gray-700 mb-4">
          No other information except for the above is logged on the mud.band server,
          and the data will not be shared with any third parties.
        </p>
        
        <p className="text-gray-700 font-medium mb-6">
          By clicking the "I agree" button, you agree to our Terms of Service
          and Privacy Policy.
        </p>  
      </div>
      <div className="flex justify-center gap-4">
        <Button onClick={async () => {
          await invoke('mudband_ui_set_user_tos_agreed', { agreed: true });
          window.location.href = "/";
        }}>I agree</Button>
        <Button variant="outline" onClick={() => {
          shell.open("https://mud.band/policy/tos");
        }}>View ToS</Button>
      </div>
    </div>
  );
}
