import { invoke } from "@tauri-apps/api/tauri";
import "../css/index.css";
import { redirect } from "react-router-dom";

export async function loader() {
  const is_user_tos_agreed = await invoke("mudband_ui_is_user_tos_agreed");
  if (!is_user_tos_agreed) {
    return redirect("/tos");
  }
  const enrollment_count = (await invoke("mudband_ui_get_enrollment_count")) as number;
  if (enrollment_count == -1) {
    return redirect("/error");
  } else if (enrollment_count == 0) {
    return redirect("/enrollment/intro");
  }
  return redirect("/dashboard");
}
  
export default function LogoPage() {
  return (
    <div className="flex h-screen">
      <div className="m-auto">
        <img src="/mudband_95x95.png"></img>
        <div className="text-center p-3">Loading...</div>
      </div>
    </div>
  );
}
