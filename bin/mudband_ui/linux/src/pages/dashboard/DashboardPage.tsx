import { useState, useEffect } from "react"

import { FontAwesomeIcon } from "@fortawesome/react-fontawesome"
import { faList } from "@fortawesome/free-solid-svg-icons"
import { invoke } from "@tauri-apps/api/tauri"
import { open } from '@tauri-apps/api/shell'
import { Activity, Smartphone, Link, Terminal, Settings } from "lucide-react"

import {
  Sheet,
  SheetContent,
  SheetHeader,
  SheetTitle,
  SheetTrigger,
} from "@/components/ui/sheet"

import DashboardStatusCard from "./DashboardStatusCard"
import DashboardDevicesCard from "./DashboardDevicesCard"
import DashboardLinksCard from "./DashboardLinksCard"
import DashboardSettingsCard from "./DashboardSettingsCard"
import { useToast } from "@/hooks/use-toast"

export default function DashboardPage() {
  const { toast } = useToast()
  const [selectedMenu, setSelectedMenu] = useState('status')
  const [isOpen, setIsOpen] = useState(false)
  const [showWebCLI, setShowWebCLI] = useState(false)

  useEffect(() => {
    const checkWebCLIAccess = async () => {
      try {
        const resp = await invoke('mudband_ui_get_active_band')
        const resp_json = JSON.parse(resp as string) as { 
          status: number, 
          msg?: string, 
          band?: { 
            name: string,
            uuid: string,
            opt_public: number,
            description: string,
            jwt: string
          } 
        }       
        if (resp_json.status !== 200 || !resp_json.band) {
          toast({
            variant: "destructive",
            title: "Error",
            description: `BANDEC_00626: Failed to get active band: ${resp_json.msg ? resp_json.msg : 'N/A'}`
          })
          return
        }
        setShowWebCLI(resp_json.band.opt_public === 1)
      } catch (error) {
        console.error('BANDEC_00627: Failed to check WebCLI access:', error)
        setShowWebCLI(false)
      }
    }
    
    checkWebCLIAccess()
  }, [])

  const handleWebCLIClick = async () => {
    try {
      const resp = await invoke('mudband_ui_get_active_band')
      const resp_json = JSON.parse(resp as string) as {
        status: number,
        band?: {
          jwt: string
        }
      }

      if (resp_json.status !== 200 || !resp_json.band) {
        toast({
          variant: "destructive",
          title: "Error",
          description: "BANDEC_00628: Failed to get active band information."
        })
        return
      }

      const response = await fetch('https://mud.band/webcli/signin', {
        method: 'GET',
        headers: {
          'Authorization': `${resp_json.band.jwt}`
        }
      })

      const data = await response.json()
      if (data.status !== 200) {
        toast({
          variant: "destructive", 
          title: "Error",
          description: `BANDEC_00629: Failed to get WebCLI URL: ${data.msg || 'N/A'}`
        })
        return
      }
      if (data.url) {
        await open(data.url)
      } else {
        toast({
          variant: "destructive",
          title: "Error", 
          description: "BANDEC_00630: Failed to open WebCLI URL."
        })
      }
    } catch (error) {
      toast({
        variant: "destructive",
        title: "Error",
        description: `BANDEC_00631: Failed to open WebCLI: ${error}`
      })
    }
  }

  const renderContent = () => {
    switch (selectedMenu) {
      case 'status':
        return <DashboardStatusCard />
      case 'devices':
        return <DashboardDevicesCard />
      case 'links':
        return <DashboardLinksCard />
      case 'settings':
        return <DashboardSettingsCard />
      default:
        return (
          <div className="bg-white p-4 rounded-lg shadow-sm">
            <h2 className="text-lg font-semibold mb-2">Unknown menu</h2>
            <p className="text-sm text-gray-600">Unknown menu selected.</p>
          </div>
        )
    }
  }

  return (
    <div className="min-h-screen bg-gray-50">
      <nav className="bg-white shadow-sm p-2 flex items-center">
        <Sheet open={isOpen} onOpenChange={setIsOpen}>
          <SheetTrigger className="hover:bg-gray-100 p-1.5 transition-colors">
            <FontAwesomeIcon icon={faList} className="w-5 h-5 text-gray-600" />
          </SheetTrigger>
          <SheetContent side="left" className="bg-white">
            <SheetHeader className="pb-2">
              <SheetTitle className="text-xl font-bold text-gray-800">Mud.band</SheetTitle>
            </SheetHeader>
            <div className="mt-4 space-y-2">
              <button 
                onClick={() => {
                  setSelectedMenu('status')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors flex items-center"
              >
                <Activity className="w-4 h-4 mr-2" />
                Status
              </button>
              <button 
                onClick={() => {
                  setSelectedMenu('devices')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors flex items-center"
              >
                <Smartphone className="w-4 h-4 mr-2" />
                Devices
              </button>
              <button 
                onClick={() => {
                  setSelectedMenu('links')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors flex items-center"
              >
                <Link className="w-4 h-4 mr-2" />
                Links
              </button>
              {showWebCLI && (
                <button 
                  onClick={() => {
                    handleWebCLIClick()
                    setIsOpen(false)
                  }}
                  className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors flex items-center"
                >
                  <Terminal className="w-4 h-4 mr-2" />
                  WebCLI
                </button>
              )}
              <button 
                onClick={() => {
                  setSelectedMenu('settings')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors flex items-center"
              >
                <Settings className="w-4 h-4 mr-2" />
                Settings
              </button>
            </div>
          </SheetContent>
        </Sheet>
        <span className="text-lg font-semibold pb-1">Mud.band</span>
      </nav>

      <main className="container mx-auto px-3 py-4">
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {renderContent()}
        </div>
      </main>
    </div>
  )
}
