import * as React from "react";
import * as ReactDOM from "react-dom/client";
import { Toaster } from "@/components/ui/toaster"

import {
  createBrowserRouter,
  RouterProvider,
} from "react-router-dom";

import LogoPage, { loader as loaderLogoPage } from "./pages/LogoPage";
import RootPage from "./pages/RootPage";
import TermsOfServicePage from "./pages/tos/TermsOfServicePage";
import DashboardPage from "./pages/dashboard/DashboardPage";
import EnrollmentNewPage from "./pages/enrollment/EnrollmentNewPage";
import EnrollmentIntroPage from "./pages/enrollment/EnrollmentIntroPage";
import ErrorPage from "./pages/ErrorPage";

const router = createBrowserRouter([
  {
    path: "/",
    element: <RootPage />,
    children: [
      {
        index: true,
        element: <LogoPage />,
        loader: loaderLogoPage
      },
      {
        path: "/dashboard",
        element: <DashboardPage />
      },
      {
        path: "/error",
        element: <ErrorPage />
      },
      {
        path: "/tos",
        element: <TermsOfServicePage />
      },
      {
        path: "/enrollment/intro",
        element: <EnrollmentIntroPage />
      },
      {
        path: "/enrollment/new",
        element: <EnrollmentNewPage />
      }
    ]
  },
]);

ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
  <React.StrictMode>
    <RouterProvider router={router} />
    <Toaster />
  </React.StrictMode>,
);
