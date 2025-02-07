import { useState } from "react"
import {
  Sheet,
  SheetContent,
  SheetHeader,
  SheetTitle,
  SheetTrigger,
} from "@/components/ui/sheet"
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome"
import { faList } from "@fortawesome/free-solid-svg-icons"
import DashboardStatusCard from "./DashboardStatusCard"
import DashboardDevicesCard from "./DashboardDevicesCard"
import DashboardLinksCard from "./DashboardLinksCard"

export default function DashboardPage() {
  const [selectedMenu, setSelectedMenu] = useState('status')
  const [isOpen, setIsOpen] = useState(false)

  const renderContent = () => {
    switch (selectedMenu) {
      case 'status':
        return <DashboardStatusCard />
      case 'devices':
        return <DashboardDevicesCard />
      case 'links':
        return <DashboardLinksCard />
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
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors"
              >
                Status
              </button>
              <button 
                onClick={() => {
                  setSelectedMenu('devices')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors"
              >
                Devices
              </button>
              <button 
                onClick={() => {
                  setSelectedMenu('links')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors"
              >
                Links
              </button>
              <button 
                onClick={() => {
                  setSelectedMenu('webcli')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors"
              >
                WebCLI
              </button>
              <button 
                onClick={() => {
                  setSelectedMenu('settings')
                  setIsOpen(false)
                }}
                className="block w-full text-left p-1.5 hover:bg-gray-100 rounded-lg transition-colors"
              >
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
