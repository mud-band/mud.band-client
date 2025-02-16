import { useEffect, useState } from "react"
import { invoke } from "@tauri-apps/api/tauri"
import { open } from '@tauri-apps/api/shell'
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/ui/card"
import { useToast } from "@/hooks/use-toast"

interface Link {
  name: string
  url: string
}

export default function DashboardLinksCard() {
  const { toast } = useToast()
  const [links, setLinks] = useState<Link[]>([])

  useEffect(() => {
    const fetchLinks = async () => {
      try {
        const conf = await invoke<string>("mudband_ui_get_active_conf")
        const conf_resp = JSON.parse(conf) as { 
          status: number,
          msg?: string,
          conf: { 
            links: Link[] 
          } 
        }
        if (conf_resp.status === 200) {
          setLinks(conf_resp.conf.links)
        } else {
          toast({
            variant: "destructive",
            title: "Error",
            description: `BANDEC_00734: Failed to fetch devices: ${conf_resp.msg ? conf_resp.msg : 'N/A'}`
          })
        }
      } catch (error) {
        toast({
          variant: "destructive",
          title: "Error",
          description: `BANDEC_00735: Failed to fetch devices: ${error}`
        })
      }
    }
    fetchLinks()
  }, [])

  const handleUrlClick = async (url: string) => {
    try {
      await open(url)
    } catch (error) {
      toast({
        variant: "destructive",
        title: "Error",
        description: `Failed to open URL: ${error}`
      })
    }
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle>Links</CardTitle>
      </CardHeader>
      <CardContent>
        <p className="text-sm text-muted-foreground mb-2">List of links.</p>
        
        <div className="space-y-2">
          {links.map((link, index) => (
            <div
              key={index}
              className="flex items-center justify-between p-2 border rounded-lg"
            >
              <div>
                <span>{link.name}</span>
                <div 
                  className="text-xs text-muted-foreground mt-1 cursor-pointer hover:underline"
                  onClick={() => handleUrlClick(link.url)}
                >
                  {link.url}
                </div>
              </div>
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  )
}