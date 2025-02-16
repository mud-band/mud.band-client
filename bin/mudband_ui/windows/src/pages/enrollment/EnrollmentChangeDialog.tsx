import { Button } from "@/components/ui/button"
import { invoke } from "@tauri-apps/api/tauri"
import { useEffect, useState } from "react"
import { useToast } from "@/hooks/use-toast"

interface Enrollment {
    name: string
    band_uuid: string
}

interface EnrollmentListResponse {
    status: number
    msg?: string
    enrollments?: Enrollment[]
}

interface EnrollmentChangeDialogProps {
    onSuccess?: () => void;
}

export default function EnrollmentChangeDialog({ onSuccess }: EnrollmentChangeDialogProps) {
    const { toast } = useToast()
    const [enrollments, setEnrollments] = useState<Enrollment[]>([]);
    const [activeBandName, setActiveBandName] = useState<string>("");

    useEffect(() => {
        const fetchData = async () => {
            try {
                const activeBandResponse = await invoke('mudband_ui_get_active_band');
                const parsedActiveBand = JSON.parse(activeBandResponse as string) as { 
                    status: number, 
                    msg?: string, 
                    band?: { 
                        name: string,
                        uuid: string,
                        opt_public: number,
                        description: string,
                        jwt: string,
                        wireguard_privkey: string
                    } 
                };
                if (parsedActiveBand.status === 200 && parsedActiveBand.band) {
                    setActiveBandName(parsedActiveBand.band.name);
                } else {
                    toast({
                        variant: "destructive",
                        title: "Error",
                        description: `BANDEC_00736: Failed to get band name: ${parsedActiveBand.msg ? parsedActiveBand.msg : 'N/A'}`
                    });
                }

                const enrollmentsResponse = await invoke('mudband_ui_get_enrollment_list');
                const parsedEnrollments = JSON.parse(enrollmentsResponse as string) as EnrollmentListResponse;
                if (parsedEnrollments.status === 200) {
                    setEnrollments(parsedEnrollments.enrollments || []);
                }
            } catch (error) {
                toast({
                    variant: "destructive",
                    title: "Error",
                    description: `BANDEC_00737: Failed to fetch data: ${error}`
                });
            }
        };

        fetchData();
    }, []);

    const handleChangeClick = async (band_uuid: string) => {
        try {
            const response = await invoke('mudband_ui_change_enrollment', { 
                bandUuid: band_uuid 
            });
            const parsedResponse = JSON.parse(response as string);
            if (parsedResponse.status === 200) {
                toast({
                    title: "Success",
                    description: "Successfully changed enrollment"
                });
                onSuccess?.();
            } else {
                toast({
                    variant: "destructive",
                    title: "Error",
                    description: parsedResponse.msg || "Failed to change enrollment"
                });
            }
        } catch (error) {
            toast({
                variant: "destructive",
                title: "Error",
                description: `BANDEC_00738: Failed to change enrollment: ${error}`
            });
        }
    };

    return (
        <div className="max-w-2xl w-full">
            <div className="mb-6">
                <h2 className="text-2xl font-bold">Change Enrollment</h2>
                <p className="text-muted-foreground">
                    Currently using: <span className="font-medium">{activeBandName}</span>
                </p>
                <p className="text-muted-foreground mt-2">
                    Select a enrollment to change
                </p>
            </div>
            
            <div className="grid grid-cols-1 gap-2">
                {enrollments.map((enrollment) => (
                    <div 
                        key={enrollment.band_uuid}
                        className="flex items-center justify-between bg-white border rounded p-2 hover:bg-slate-50 transition-colors"
                    >
                        <span className="font-medium">{enrollment.name}</span>
                        <Button 
                            variant="secondary" 
                            onClick={() => handleChangeClick(enrollment.band_uuid)}
                        >
                            Change
                        </Button>
                    </div>
                ))}
            </div>
        </div>
    )
}
