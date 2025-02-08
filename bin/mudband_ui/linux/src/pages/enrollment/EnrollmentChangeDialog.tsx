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

    useEffect(() => {
        const fetchEnrollments = async () => {
            try {
                const response = await invoke('mudband_ui_get_enrollment_list');
                const parsedResponse = JSON.parse(response as string) as EnrollmentListResponse;
                if (parsedResponse.status === 200) {
                    setEnrollments(parsedResponse.enrollments || []);
                }
            } catch (error) {
                toast({
                    variant: "destructive",
                    title: "Error",
                    description: `BANDEC_XXXXX: Failed to fetch enrollments: ${error}`
                });
            }
        };

        fetchEnrollments();
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
                description: `BANDEC_XXXXX: Failed to change enrollment: ${error}`
            });
        }
    };

    return (
        <div className="max-w-2xl w-full">
            <div className="mb-6">
                <h2 className="text-2xl font-bold">Change Enrollment</h2>
                <p className="text-muted-foreground">
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
