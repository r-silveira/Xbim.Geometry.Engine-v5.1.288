#include "XbimFace.h"
#include "XbimOccWriter.h"
#include "XbimCurve.h"
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <BRepCheck_Analyzer.hxx>
#include "XbimGeometryCreator.h"
#include "XbimConvert.h" 
#include <TopExp_Explorer.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepTools.hxx>
#include <TopoDS.hxx>
#include <GProp_GProps.hxx>
#include <BRepGProp.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <BRepGProp_Face.hxx>
#include <gp_Circ.hxx>
#include <GC_MakeCircle.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Surface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <gp_Pln.hxx>
#include <BRepAdaptor_CompCurve.hxx>
#include <Geom_SurfaceOfRevolution.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <gp_Cylinder.hxx>
#include <BRepFilletAPI_MakeFillet2d.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Wire.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepTopAdaptor_FClass2d.hxx>
#include <BRepBuilderAPI_FindPlane.hxx>
#include <Geom_Plane.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <GProp_PGProps.hxx>
#include <ShapeFix_Edge.hxx>
#include <ShapeAnalysis.hxx>

using namespace System::Linq;

namespace Xbim
{
	namespace Geometry
	{
		/*Ensures native pointers are deleted and garbage collected*/
		void XbimFace::InstanceCleanup()
		{
			IntPtr temp = System::Threading::Interlocked::Exchange(ptrContainer, IntPtr::Zero);
			if (temp != IntPtr::Zero)
				delete (TopoDS_Face*)(temp.ToPointer());
			System::GC::SuppressFinalize(this);
		}

		String^ XbimFace::GetBuildFaceErrorMessage(BRepBuilderAPI_FaceError err)
		{
			switch (err)
			{
			case BRepBuilderAPI_NoFace:
				return "No Face";
			case BRepBuilderAPI_NotPlanar:
				return "Not Planar";
			case BRepBuilderAPI_CurveProjectionFailed:
				return "Curve Projection Failed";
			case BRepBuilderAPI_ParametersOutOfRange:
				return "Parameters Out Of Range";
			case BRepBuilderAPI_FaceDone:
				return "";
			default:
				return "Unknown Error";
			}
		}
		bool XbimFace::RemoveDuplicatePoints(TColgp_SequenceOfPnt& polygon, bool closed, double tol)
		{
			tol *= tol;
			bool isClosed = false;
			while (true) {
				bool removed = false;
				int n = polygon.Length() - (closed ? 0 : 1);
				for (int i = 1; i <= n; ++i) {
					// wrap around to the first point in case of a closed loop
					int j = (i % polygon.Length()) + 1;
					double dist = polygon.Value(i).SquareDistance(polygon.Value(j));
					if (dist < tol) {
						if (j == 1 && i == n) //the first and last point are the same
							isClosed = true;
						// do not remove the first or last point to
						// maintain connectivity with other wires
						if ((closed && j == 1) || (!closed && j == n)) 
							polygon.Remove(i);
						else 
							polygon.Remove(j);
						removed = true;
						break;
					}
				}
				if (!removed) break;
			}
			
			return isClosed;
		}

		XbimFace::XbimFace(XbimPoint3D l, XbimVector3D n, ILogger^ /*logger*/)
		{
			gp_Pln plane(gp_Pnt(l.X, l.Y, l.Z), gp_Dir(n.X, n.Y, n.Z));
			BRepBuilderAPI_MakeFace faceMaker(plane);
			pFace = new TopoDS_Face();
			*pFace = faceMaker.Face();
		}

		XbimFace::XbimFace(XbimVector3D n, ILogger^ /*logger*/)
		{
			gp_Pln plane(gp_Pnt(0, 0, 0), gp_Dir(n.X, n.Y, n.Z));
			BRepBuilderAPI_MakeFace faceMaker(plane);
			pFace = new TopoDS_Face();
			*pFace = faceMaker.Face();
		}

		XbimFace::XbimFace(const TopoDS_Face& face)
		{
			pFace = new TopoDS_Face();
			*pFace = face;
		}

		XbimFace::XbimFace(const TopoDS_Face& face, Object^ tag) : XbimFace(face)
		{
			Tag = tag;
		}

		XbimFace::XbimFace(IIfcProfileDef^ profile, ILogger^ logger)
		{
			Init(profile, logger);
		}


		XbimFace::XbimFace(IIfcSurface^ surface, ILogger^ logger)
		{
			Init(surface, logger);
		}

		XbimFace::XbimFace(IIfcCurveBoundedPlane^ def, ILogger^ logger)
		{
			Init(def, logger);
		}

		XbimFace::XbimFace(IIfcRectangularTrimmedSurface^ def, ILogger^ logger)
		{
			Init(def, logger);
		}

		XbimFace::XbimFace(IIfcPlane^ plane, ILogger^ logger)
		{
			Init(plane, logger);
		}

		XbimFace::XbimFace(IIfcCylindricalSurface^ cylinder, ILogger^ logger)
		{
			Init(cylinder, logger);
		}

		XbimFace::XbimFace(IIfcSurfaceOfLinearExtrusion^ sLin, ILogger^ logger)
		{
			Init(sLin, logger);
		}

		XbimFace::XbimFace(IIfcSurfaceOfRevolution^ sRev, ILogger^ logger)
		{
			Init(sRev, logger);
		}
		XbimFace::XbimFace(IIfcCompositeCurve ^ cCurve, ILogger^ logger)
		{
			Init(cCurve, logger);
		}

		XbimFace::XbimFace(IIfcPolyline ^ pline, ILogger^ logger)
		{
			Init(pline, logger);
		}

		XbimFace::XbimFace(IIfcPolyLoop ^ loop, ILogger^ logger)
		{
			Init(loop, logger);
		}


		XbimFace::XbimFace(IXbimWire^ wire, bool isPlanar, double precision, int entityLabel, ILogger^ logger)
		{

			Init(wire, isPlanar, precision, entityLabel, logger);
		}

		XbimFace::XbimFace(IXbimWire^ wire, XbimPoint3D pointOnFace, XbimVector3D faceNormal, ILogger^ logger)
		{
			Init(wire, pointOnFace, faceNormal, logger);
		}
		XbimFace::XbimFace(IIfcFace^ face, ILogger^ logger)
		{
			Init(face, logger);
		}


		XbimFace::XbimFace(double x, double y, double tolerance, ILogger^ logger)
		{
			Init(x, y, tolerance, logger);
		}

		XbimFace::XbimFace(IIfcSurface^ surface, XbimWire^ outerBound, IEnumerable<XbimWire^>^ innerBounds, ILogger^ logger)
		{
			Init(surface, logger);
			if (!IsValid) return;

			Handle(Geom_Surface) geomSurface = BRep_Tool::Surface(this);
			BRepBuilderAPI_MakeFace faceMaker(geomSurface, outerBound, Standard_True);
			if (faceMaker.IsDone())
			{
				for each (XbimWire^ inner in innerBounds)
				{
					faceMaker.Add(inner);
				}
				BRepCheck_Analyzer analyser(faceMaker.Face(), Standard_False);
				if (!analyser.IsValid())
				{
					ShapeFix_Face sfs(faceMaker.Face());
					sfs.Perform();
					*pFace = sfs.Face();
				}
				else
					*pFace = faceMaker.Face();
			}
			else
			{
				delete pFace;
				pFace = nullptr;
			}
		}

		//NB the wires defined in the facesurface are ignored
		XbimFace::XbimFace(IIfcFaceSurface^ surface, XbimWire^ outerBound, IEnumerable<XbimWire^>^ innerBounds, double tolerance, ILogger^ logger)
		{
			Init(surface->FaceSurface, logger);
			if (!IsValid) return;
			//make sure all the pcurves are built	
			ShapeFix_Wire wFix(outerBound, this, tolerance);
			wFix.FixEdgeCurves();
			XbimWire^ w = gcnew XbimWire(wFix.Wire());
			//check orientation
			if (!surface->SameSense) w = w->Reversed();
			Handle(Geom_Surface) geomSurface = BRep_Tool::Surface(this);
			BRepBuilderAPI_MakeFace faceMaker(geomSurface, w, Standard_True);

			if (faceMaker.IsDone())
			{
				for each (XbimWire^ inner in innerBounds)
				{
					wFix.Init(inner, faceMaker.Face(), tolerance * 10);
					wFix.FixEdgeCurves();
					//	XbimWire^ w = gcnew XbimWire(wFix.Wire());
						//if (surface->SameSense) w = w->Reversed();
					//	faceMaker.Add(inner);
					TopoDS_Face newface = TopoDS::Face(faceMaker.Face().EmptyCopied().Oriented(TopAbs_FORWARD));
					BRep_Builder b;
					b.Add(newface, wFix.Wire());
					BRepTopAdaptor_FClass2d fClass2d(newface, Precision::PConfusion());
					if (fClass2d.PerformInfinitePoint() != TopAbs_IN) //material shuld be on the outside of the wire
						faceMaker.Add(TopoDS::Wire(wFix.Wire().Reversed()));
					else faceMaker.Add(wFix.Wire());
				}
				*pFace = faceMaker.Face();
				//the code below helps find errors in orientation, only for debugging
				//Quantity_Parameter uMin;
				//Quantity_Parameter vMin;
				//Quantity_Parameter uMax;
				//Quantity_Parameter vMax;
				//BRepTools::UVBounds(*pFace, uMin, uMax, vMin, vMax);
				//if (!geomSurface->IsUPeriodic()) 
				//	uMax += 1;
				//if (!geomSurface->IsVPeriodic()) 
				//	vMax += 1;
				//gp_Pnt2d p2d(Math::Min(uMax, Precision::Infinite()), Math::Min(vMax,Precision::Infinite())); //pick a point that shhould be outside the natural bounds
				//BRepClass_FaceClassifier fc(*pFace, p2d, surface->Model->ModelFactors->Precision);
				//TopAbs_State state = fc.State();
				//if (state == TopAbs_IN)
				//{
				//	//flip this should not happen but some models have bugs
				//	Console::WriteLine("In");
				//}
				//if (state == TopAbs_OUT)
				//{

				//	Console::WriteLine("Out");
				//}
				//if (state == TopAbs_ON)
				//{
				//	Console::WriteLine("On");
				//}
				//if (state == TopAbs_UNKNOWN)
				//{
				//	Console::WriteLine("Unknown");
				//}
				//



			}
			else
			{
				delete pFace;
				pFace = nullptr;
			}
		}

		void XbimFace::Init(IIfcCompositeCurve ^ cCurve, ILogger^ logger)
		{
			XbimWire^ wire = gcnew XbimWire(cCurve, logger);
			if (wire->IsValid)
			{
				BRepBuilderAPI_MakeFace faceMaker(wire);
				pFace = new TopoDS_Face();
				*pFace = faceMaker.Face();
			}
		}

		void XbimFace::Init(IIfcPolyline ^ pline, ILogger^ logger)
		{

			XbimWire^ wire = gcnew XbimWire(pline, logger);
			if (wire->IsValid)
			{
				BRepBuilderAPI_MakeFace faceMaker(wire);
				pFace = new TopoDS_Face();
				*pFace = faceMaker.Face();
			}
		}

		void XbimFace::Init(IIfcPolyLoop ^ polyloop, ILogger^ logger)
		{
			List<IIfcCartesianPoint^>^ polygon = Enumerable::ToList(polyloop->Polygon);
			int originalCount = polygon->Count;
			double tolerance = polyloop->Model->ModelFactors->Precision;
			if (originalCount < 3)
			{
				XbimGeometryCreator::LogWarning(logger, polyloop, "Invalid loop, it has less than three points. Wire discarded");
				return;
			}

			TColgp_SequenceOfPnt pointSeq;
			BRepBuilderAPI_MakeWire wireMaker;
			for (int i = 0; i < originalCount; i++)
			{
				pointSeq.Append(XbimConvert::GetPoint3d(polygon[i]));
			}

			XbimFace::RemoveDuplicatePoints(pointSeq, true, tolerance);

			if (pointSeq.Length() != originalCount)
			{
				XbimGeometryCreator::LogInfo(logger, polyloop, "Polyloop with duplicate points. Ifc rule: first point shall not be repeated at the end of the list. It has been removed");
			}

			if (pointSeq.Length() < 3)
			{
				XbimGeometryCreator::LogWarning(logger, polyloop, "Polyloop with less than 3 points is an empty loop. It has been ignored");
				return;
			}
			//get the basic properties
			TColgp_Array1OfPnt pointArray(1, pointSeq.Length());
			for (int i = 1; i <= pointSeq.Length(); i++)
			{
				pointArray.SetValue(i, pointSeq.Value(i));
			}


			//limit the tolerances for the vertices and edges
			BRepBuilderAPI_MakePolygon polyMaker;
			for (int i = 1; i <= pointSeq.Length(); ++i) {
				polyMaker.Add(pointSeq.Value(i));
			}
			polyMaker.Close();

			if (polyMaker.IsDone())
			{
				bool isPlanar;
				gp_Vec normal = XbimConvert::NewellsNormal(pointArray, isPlanar);
				if (!isPlanar)
				{

					XbimGeometryCreator::LogInfo(logger, polyloop, "Polyloop is a line. Empty loop built");
					return;
				}
				gp_Pnt centre = GProp_PGProps::Barycentre(pointArray);
				gp_Pln thePlane(centre, normal);
				TopoDS_Wire theWire = polyMaker.Wire();
				TopoDS_Face theFace = BRepBuilderAPI_MakeFace(thePlane, theWire, false);

				//limit the tolerances for the vertices and edges
				ShapeFix_ShapeTolerance tolFixer;
				tolFixer.LimitTolerance(theWire, tolerance); //set all tolerances
				//adjust vertex tolerances for bad planar fit if we have anything more than a triangle (which will alway fit a plane)

				if (pointSeq.Length() > 3)
				{
					TopTools_IndexedMapOfShape map;
					TopExp::MapShapes(theWire, TopAbs_EDGE, map);
					ShapeFix_Edge ef;
					bool fixed = false;
					for (int i = 1; i <= map.Extent(); i++)
					{
						const TopoDS_Edge edge = TopoDS::Edge(map(i));
						if (ef.FixVertexTolerance(edge, theFace)) fixed = true;

					}
					if (fixed)
						XbimGeometryCreator::LogInfo(logger, polyloop, "Polyloop is slightly mis-aligned to a plane. It has been adjusted");
				}
				//need to check for self intersecting edges to comply with Ifc rules
				double maxTol = BRep_Tool::MaxTolerance(theWire, TopAbs_VERTEX);
				Handle(ShapeAnalysis_Wire) wa = new ShapeAnalysis_Wire(theWire, theFace, maxTol);

				if (wa->CheckSelfIntersection()) //some edges are self intersecting or not on plane within tolerance, fix them
				{
					ShapeFix_Wire wf;
					wf.Init(wa);
					wf.SetPrecision(tolerance);
					wf.SetMinTolerance(tolerance);
					wf.SetMaxTolerance(maxTol);
					if (!wf.Perform())
					{
						XbimGeometryCreator::LogWarning(logger, polyloop, "Failed to fix self-interecting wire edges");

					}
					else
					{
						theWire = wf.Wire();
						theFace = BRepBuilderAPI_MakeFace(thePlane, theWire, false);
					}

				}
				pFace = new TopoDS_Face();
				*pFace = theFace;

			}
			else
			{
				XbimGeometryCreator::LogWarning(logger, polyloop, "Failed to build Polyloop"); //nothing more to say to the log			
			}

		}

		void XbimFace::Init(IXbimWire^ xbimWire, XbimPoint3D pointOnFace, XbimVector3D faceNormal, ILogger^ /*logger*/)
		{
			if (!dynamic_cast<XbimWire^>(xbimWire))
				throw gcnew ArgumentException("Only IXbimWires created by Xbim.OCC modules are supported", "xbimWire");
			XbimWire^ wire = (XbimWire^)xbimWire;
			if (wire->IsValid && !faceNormal.IsInvalid())
			{
				gp_Pln plane(gp_Pnt(pointOnFace.X, pointOnFace.Y, pointOnFace.Z), gp_Dir(faceNormal.X, faceNormal.Y, faceNormal.Z));
				BRepBuilderAPI_MakeFace faceMaker(plane, wire, Standard_False);
				TopoDS_Face resultFace = faceMaker.Face();
				if (BRepCheck_Analyzer(resultFace, Standard_True).IsValid() == Standard_False)
				{
					ShapeFix_Face faceFixer(faceMaker.Face());
					faceFixer.Perform();
					resultFace = faceFixer.Face();
				}
				pFace = new TopoDS_Face();
				*pFace = resultFace;
			}
			GC::KeepAlive(xbimWire);
		}

		void XbimFace::Init(IXbimWire^ xbimWire, bool isPlanar, double precision, int entityLabel, ILogger^ logger)
		{
			if (!dynamic_cast<XbimWire^>(xbimWire))
				throw gcnew ArgumentException("Only IXbimWires created by Xbim.OCC modules are supported", "xbimWire");
			XbimWire^ wire = (XbimWire^)xbimWire;
			double currentPrecision = precision;
			if (wire->IsValid)
			{
				if (isPlanar)
				{
					int retriedMakePlaneCount = 0;
					//we need to find the correct plane
				makePlane:
					BRepBuilderAPI_FindPlane planeMaker(wire, currentPrecision);
					if (retriedMakePlaneCount < 20 && !planeMaker.Found())
					{
						retriedMakePlaneCount++;
						currentPrecision = 10 * precision * retriedMakePlaneCount;
						goto makePlane;
					}
					if (!planeMaker.Found())
						XbimGeometryCreator::LogError(logger, nullptr, "Failure to build planar face due to a non-planar wire, entity #{0}", entityLabel);
					else
					{
						BRepBuilderAPI_MakeFace faceMaker(planeMaker.Plane()->Pln(), wire, Standard_True);
						pFace = new TopoDS_Face();
						*pFace = faceMaker.Face();
					}
				}
				else
				{
					BRepBuilderAPI_MakeFace faceMaker(wire, Standard_False);
					if (faceMaker.IsDone())
					{
						pFace = new TopoDS_Face();
						*pFace = faceMaker.Face();
					}
					else
					{
						XbimGeometryCreator::LogError(logger, nullptr, "Failure to build non-planar face, entity #{0}", entityLabel);
					}
				}
			}
			GC::KeepAlive(xbimWire);
		}

		void XbimFace::Init(IIfcFace^ ifcFace, ILogger^ logger)
		{
			double tolerance = ifcFace->Model->ModelFactors->Precision;
			double angularTolerance = 0.00174533; //1 tenth of a degree
			double outerLoopArea = 0;
			ShapeFix_ShapeTolerance tolFixer;
			TopoDS_Face theFace;
			TopoDS_ListOfShape innerBounds;
			for each (IIfcFaceBound^ bound in ifcFace->Bounds)
			{
				IIfcPolyLoop^ polyloop = dynamic_cast<IIfcPolyLoop^>(bound->Bound);

				if (polyloop == nullptr || !XbimConvert::IsPolygon((IIfcPolyLoop^)bound->Bound))
				{
					XbimGeometryCreator::LogInfo(logger, bound, "Polyloop bound is not a polygon and has been ignored");
					continue; //skip non-polygonal faces
				}
				//List<IIfcCartesianPoint^>^ polygon = Enumerable::ToList(polyloop->Polygon);
				int originalCount = polyloop->Polygon->Count;

				if (originalCount < 3)
				{
					XbimGeometryCreator::LogWarning(logger, polyloop, "Invalid loop, it has less than three points. Wire discarded");
					continue;
				}

				TColgp_SequenceOfPnt pointSeq;
				BRepBuilderAPI_MakeWire wireMaker;
				for (int i = 0; i < originalCount; i++)
				{
					pointSeq.Append(XbimConvert::GetPoint3d(polyloop->Polygon[i]));
				}

				XbimFace::RemoveDuplicatePoints(pointSeq, true, tolerance);

				if (pointSeq.Length() != originalCount)
				{
					XbimGeometryCreator::LogInfo(logger, polyloop, "Polyloop with duplicate points. Ifc rule: first point shall not be repeated at the end of the list. It has been removed");
				}

				if (pointSeq.Length() < 3)
				{
					XbimGeometryCreator::LogWarning(logger, polyloop, "Polyloop with less than 3 points is an empty loop. It has been ignored");
					continue;
				}
				//get the basic properties
				TColgp_Array1OfPnt pointArray(1, pointSeq.Length());
				for (int i = 1; i <= pointSeq.Length(); i++)
				{
					pointArray.SetValue(i, pointSeq.Value(i));
				}


				//limit the tolerances for the vertices and edges
				BRepBuilderAPI_MakePolygon polyMaker;
				if (!bound->Orientation) //reverse the points
				{
					for (int i = pointSeq.Length(); i > 0; i--) {
						polyMaker.Add(pointSeq.Value(i));
					}
				}
				else
				{
					for (int i = 1; i <= pointSeq.Length(); ++i) {
						polyMaker.Add(pointSeq.Value(i));
					}
				}
				
				polyMaker.Close();
				
				
				if (polyMaker.IsDone())
				{
					
					bool isPlanar;
					gp_Vec normal = XbimConvert::NewellsNormal(pointArray, isPlanar);
					if (!isPlanar)
					{
						XbimGeometryCreator::LogInfo(logger, polyloop, "Polyloop is a line. Empty loop built");
						continue;
					}
					gp_Pnt centre = GProp_PGProps::Barycentre(pointArray);
					gp_Pln thePlane(centre, normal);
					TopoDS_Wire theWire = polyMaker.Wire();
					tolFixer.LimitTolerance(theWire, tolerance); //set all tolerances
					
					TopoDS_Face aFace = BRepBuilderAPI_MakeFace(thePlane, theWire,false);
					//need to check for self intersecting edges to comply with Ifc rules
					TopTools_IndexedMapOfShape map;
					TopExp::MapShapes(aFace, TopAbs_EDGE, map);
					ShapeFix_Edge ef;
					bool fixed = false;
					for (int i = 1; i <= map.Extent(); i++)
					{
						const TopoDS_Edge edge = TopoDS::Edge(map(i));
						if (ef.FixVertexTolerance(edge, aFace)) fixed = true;
					}
					if (fixed)
						XbimGeometryCreator::LogDebug(logger, ifcFace, "Face bounds are slightly mis-aligned to a plane. It has been adjusted");
					double maxTol = BRep_Tool::MaxTolerance(theWire, TopAbs_VERTEX);
					Handle(ShapeAnalysis_Wire) wa = new ShapeAnalysis_Wire(theWire, aFace, maxTol);

					if (wa->CheckSelfIntersection()) //some edges are self intersecting or not on plane within tolerance, fix them
					{
						ShapeFix_Wire wf;
						wf.Init(wa);
						wf.SetPrecision(tolerance);
						wf.SetMinTolerance(tolerance);
						wf.SetMaxTolerance(std::max(tolerance, maxTol));
						if (!wf.Perform())
						{
							XbimGeometryCreator::LogWarning(logger, polyloop, "Failed to fix self-interecting wire edges");
						}
						else
						{
							theWire = wf.Wire();
							aFace = BRepBuilderAPI_MakeFace(thePlane, theWire, false);
						}

					}
					
					double area = ShapeAnalysis::ContourArea(theWire);
					if (area > outerLoopArea)
					{
						if (!theFace.IsNull())
						{
							innerBounds.Append(theFace); //save the face to go inside as a loop
						}
						theFace = aFace;
						outerLoopArea = area;
					}
					else
					{
						innerBounds.Append(aFace);
					}	
				}
				else
				{
					XbimGeometryCreator::LogWarning(logger, polyloop, "Failed to build Polyloop"); //nothing more to say to the log			
				}
			}
			if (!theFace.IsNull() && innerBounds.Size() > 0)
			{
				//add the other wires to the face
				BRepBuilderAPI_MakeFace faceMaker(theFace);
				
				TopoDS_ListIteratorOfListOfShape wireIter(innerBounds);
				BRepGProp_Face prop(theFace);
				gp_Pnt centre;
				gp_Vec theFaceNormal;
				double u1, u2, v1, v2;
				prop.Bounds(u1, u2, v1, v2);
				prop.Normal((u1 + u2) / 2.0, (v1 + v2) / 2.0, centre, theFaceNormal);	
				
				while (wireIter.More()) 
				{
					TopoDS_Face face = TopoDS::Face(wireIter.Value());					
					BRepGProp_Face fprop(face);				
					gp_Vec innerBoundNormalDir;
					fprop.Bounds(u1, u2, v1, v2);
					fprop.Normal((u1 + u2) / 2.0, (v1 + v2) / 2.0, centre, innerBoundNormalDir);	
					
					if (!theFaceNormal.IsOpposite(innerBoundNormalDir, angularTolerance))
					{
						
						face.Reverse();
					}
					faceMaker.Add(BRepTools::OuterWire(face));
					wireIter.Next();
				}
				theFace = faceMaker.Face();
				//limit the tolerances for the vertices and edges
				
				tolFixer.LimitTolerance(theFace, tolerance); //set all tolerances
				//adjust vertex tolerances for bad planar fit if we have anything more than a triangle (which will alway fit a plane)


				TopTools_IndexedMapOfShape map;
				TopExp::MapShapes(theFace, TopAbs_EDGE, map);
				ShapeFix_Edge ef;
				bool fixed = false;
				for (int i = 1; i <= map.Extent(); i++)
				{
					const TopoDS_Edge edge = TopoDS::Edge(map(i));
					if (ef.FixVertexTolerance(edge, theFace)) fixed = true;

				}
				if (fixed)
					XbimGeometryCreator::LogDebug(logger, ifcFace, "Face bounds are slightly mis-aligned to a plane. It has been adjusted");

				
			}
			else
			{
				tolFixer.LimitTolerance(theFace, tolerance); //set all tolerances
			}
			pFace = new TopoDS_Face();
			*pFace = theFace;
		}

		void XbimFace::Init(IXbimFace^ face, ILogger^ /*logger*/)
		{
			IXbimWire^ outerBound = face->OuterBound;
			XbimWire^ outerWire = dynamic_cast<XbimWire^>(outerBound);
			if (outerWire == nullptr) throw gcnew ArgumentException("Only IXbimWires created by Xbim.OCC modules are supported", "xbimWire");
			XbimVector3D n = face->Normal;
			XbimPoint3D pw = outerWire->Vertices->First->VertexGeometry;
			gp_Pln plane(gp_Pnt(pw.X, pw.Y, pw.Z), gp_Dir(n.X, n.Y, n.Z));
			BRepBuilderAPI_MakeFace faceMaker(plane, outerWire, Standard_False);
			for each (IXbimWire^ innerBound in face->InnerBounds)
			{
				XbimWire^ innerWire = dynamic_cast<XbimWire^>(innerBound);
				if (innerWire != nullptr)
					faceMaker.Add(innerWire);
			}
			pFace = new TopoDS_Face();
			*pFace = faceMaker.Face();
		}

		void XbimFace::Init(IIfcProfileDef^ profile, ILogger^ logger)
		{
			IIfcArbitraryProfileDefWithVoids^ arbProfDefVoids = dynamic_cast<IIfcArbitraryProfileDefWithVoids^>(profile);
			if (arbProfDefVoids != nullptr) //this is a compound wire so we need to build it at face level
				return Init(arbProfDefVoids, logger);
			IIfcCircleHollowProfileDef^ circHollow = dynamic_cast<IIfcCircleHollowProfileDef^>(profile);
			if (circHollow != nullptr) //this is a compound wire so we need to build it at face level
				return Init(circHollow, logger);
			IIfcRectangleHollowProfileDef^ rectHollow = dynamic_cast<IIfcRectangleHollowProfileDef^>(profile);
			if (rectHollow != nullptr) //this is a compound wire so we need to build it at face level
				return Init(rectHollow, logger);
			if (dynamic_cast<IIfcCompositeProfileDef^>(profile))
				return Init((IIfcCompositeProfileDef^)profile, logger);
			if (dynamic_cast<IIfcDerivedProfileDef^>(profile))
				return Init((IIfcDerivedProfileDef^)profile, logger);
			if (dynamic_cast<IIfcArbitraryOpenProfileDef^>(profile) && !dynamic_cast<IIfcCenterLineProfileDef^>(profile))
				XbimGeometryCreator::LogError(logger, profile, "Faces cannot be built with IIfcArbitraryOpenProfileDef, a face requires a closed loop");
			else //it is a standard profile that can be built as a single wire
			{
				XbimWire^ wire = gcnew XbimWire(profile, logger);
				if (wire->IsValid)
				{
					double tolerance = profile->Model->ModelFactors->Precision;
					
					XbimVector3D n = wire->Normal;
					if (n.IsInvalid()) //it is not an area
					{
						XbimGeometryCreator::LogWarning(logger, profile, "Face cannot be built with a profile that has no area.");
						return;
					}
					else
					{
						XbimPoint3D centre = wire->BaryCentre;
						gp_Pln thePlane(gp_Pnt(centre.X, centre.Y, centre.Z), gp_Vec(n.X, n.Y, n.Z));
						pFace = new TopoDS_Face();
						*pFace = BRepBuilderAPI_MakeFace(thePlane, wire, false);
						//need to check for self intersecting edges to comply with Ifc rules
						TopTools_IndexedMapOfShape map;
						TopExp::MapShapes(*pFace, TopAbs_EDGE, map);
						ShapeFix_Edge ef;

						for (int i = 1; i <= map.Extent(); i++)
						{
							const TopoDS_Edge edge = TopoDS::Edge(map(i));
							ef.FixVertexTolerance(edge, *pFace);
						}
						ShapeFix_ShapeTolerance fTol;
						fTol.LimitTolerance(*pFace, tolerance);
					}
				}
				
			}
		}

		void XbimFace::Init(IIfcDerivedProfileDef^ profile, ILogger^ logger)
		{
			Init(profile->ParentProfile, logger);
			if (IsValid && !dynamic_cast<IIfcMirroredProfileDef^>(profile))
			{
				gp_Trsf aTrsf = XbimConvert::ToTransform(profile->Operator);
				// pFace->Move(TopLoc_Location(trsf));
				BRepBuilderAPI_Transform aBrepTrsf(*pFace, aTrsf);
				*pFace = TopoDS::Face(aBrepTrsf.Shape());
			}
			if (IsValid && dynamic_cast<IIfcMirroredProfileDef^>(profile))
			{
				//we need to mirror about the Y axis
				gp_Pnt origin(0, 0, 0);
				gp_Dir xDir(0, 1, 0);
				gp_Ax1 mirrorAxis(origin, xDir);
				gp_Trsf aTrsf;
				aTrsf.SetMirror(mirrorAxis);
				BRepBuilderAPI_Transform aBrepTrsf(*pFace, aTrsf);
				*pFace = TopoDS::Face(aBrepTrsf.Shape());
				Reverse();//correct the normal to be correct
			}
		}

		void XbimFace::Init(IIfcArbitraryProfileDefWithVoids^ profile, ILogger^ logger)
		{
			double tolerance = profile->Model->ModelFactors->Precision;
			double toleranceMax = profile->Model->ModelFactors->PrecisionMax;
			ShapeFix_ShapeTolerance FTol;
			TopoDS_Face face;
			XbimWire^ loop = gcnew XbimWire(profile->OuterCurve, logger);
			if (loop->IsValid)
			{
				if (!loop->IsClosed ) //we need to close it i
				{
					double oneMilli = profile->Model->ModelFactors->OneMilliMeter;
					XbimFace^ xface = gcnew XbimFace(loop, true, oneMilli, profile->OuterCurve->EntityLabel, logger);
					ShapeFix_Wire wireFixer(loop, xface, profile->Model->ModelFactors->Precision);
					wireFixer.ClosedWireMode() = Standard_True;
					wireFixer.FixGaps2dMode() = Standard_True;
					wireFixer.FixGaps3dMode() = Standard_True;
					wireFixer.ModifyGeometryMode() = Standard_True;
					wireFixer.SetMinTolerance(profile->Model->ModelFactors->Precision);
					wireFixer.SetPrecision(oneMilli);
					wireFixer.SetMaxTolerance(oneMilli * 10);
					Standard_Boolean closed = wireFixer.Perform();
					if (closed)
						loop = gcnew XbimWire(wireFixer.Wire());
				}
				double currentFaceTolerance = tolerance;
			TryBuildFace:
				BRepBuilderAPI_MakeFace faceMaker(loop, true);
				BRepBuilderAPI_FaceError err = faceMaker.Error();
				if (err == BRepBuilderAPI_NotPlanar)
				{
					currentFaceTolerance *= 10;
					if (currentFaceTolerance <= toleranceMax)
					{
						FTol.SetTolerance(loop, currentFaceTolerance, TopAbs_WIRE);
						goto TryBuildFace;
					}
					String^ errMsg = XbimFace::GetBuildFaceErrorMessage(err);
					XbimGeometryCreator::LogWarning(logger, profile, "Invalid bound, {0}. Face discarded", errMsg);
					return;
				}
				pFace = new TopoDS_Face();
				*pFace = faceMaker.Face();
				XbimVector3D tn = Normal;

				for each(IIfcCurve^ curve in profile->InnerCurves)
				{
					XbimWire^ innerWire = gcnew XbimWire(curve, logger);
					if (!innerWire->IsClosed ) //we need to close it if we have more thn one edge
					{
						double oneMilli = profile->Model->ModelFactors->OneMilliMeter;
						XbimFace^ xface = gcnew XbimFace(innerWire, true, oneMilli, curve->EntityLabel, logger);
						ShapeFix_Wire wireFixer(innerWire, xface, profile->Model->ModelFactors->Precision);
						wireFixer.ClosedWireMode() = Standard_True;
						wireFixer.FixGaps2dMode() = Standard_True;
						wireFixer.FixGaps3dMode() = Standard_True;
						wireFixer.ModifyGeometryMode() = Standard_True;
						wireFixer.SetMinTolerance(profile->Model->ModelFactors->Precision);
						wireFixer.SetPrecision(oneMilli);
						wireFixer.SetMaxTolerance(oneMilli * 10);
						Standard_Boolean closed = wireFixer.Perform();
						if (closed)
							innerWire = gcnew XbimWire(wireFixer.Wire());
					}
					if (innerWire->IsClosed) //if the loop is not closed it is not a bound
					{
						XbimVector3D n = innerWire->Normal;
						bool needInvert = n.DotProduct(tn) > 0;
						if (needInvert) //inner wire should be reverse of outer wire
							innerWire->Reverse();
						double currentloopTolerance = tolerance;
					TryBuildLoop:
						faceMaker.Add(innerWire);
						BRepBuilderAPI_FaceError loopErr = faceMaker.Error();
						if (loopErr != BRepBuilderAPI_FaceDone)
						{
							currentloopTolerance *= 10; //try courser tolerance
							if (currentloopTolerance <= toleranceMax)
							{
								FTol.SetTolerance(innerWire, currentloopTolerance, TopAbs_WIRE);
								goto TryBuildLoop;
							}

							String^ errMsg = XbimFace::GetBuildFaceErrorMessage(loopErr);
							XbimGeometryCreator::LogWarning(logger, profile, "Invalid void, {0}. IfcCurve #{1} could not be added. Inner bound ignored", errMsg, curve->EntityLabel);
						}
						*pFace = faceMaker.Face();
					}
					else
					{
						XbimGeometryCreator::LogWarning(logger, profile, "Invalid void. IfcCurve #{0} is not closed. Inner bound ignored", curve->EntityLabel);
					}
				}
			}
		}
		void XbimFace::Init(IIfcCompositeProfileDef ^ compProfile, ILogger^ logger)
		{
			int profileCount = Enumerable::Count(compProfile->Profiles);
			if (profileCount == 0)
			{
				XbimGeometryCreator::LogInfo(logger, compProfile, "A composite profile must have 2 or more profiles, 0 were found. Profile discarded");
				return;
			}
			if (profileCount == 1)
			{
				XbimGeometryCreator::LogInfo(logger, compProfile, "A composite profile must have 2 or more profiles, 1 was found. A prilfe with a single segment has been used");
				Init(Enumerable::First(compProfile->Profiles), logger);
				return;
			}
			XbimFace^ firstFace = gcnew XbimFace(Enumerable::First(compProfile->Profiles), logger);
			BRepBuilderAPI_MakeFace faceBlder(firstFace);
			bool first = true;
			for each (IIfcProfileDef^ profile in compProfile->Profiles)
			{
				if (!first)
				{
					XbimFace^ face = gcnew XbimFace(profile, logger);
					faceBlder.Add((XbimWire^)face->OuterBound);
					for each (IXbimWire^ inner in face->InnerBounds)
					{
						faceBlder.Add((XbimWire^)inner);
					}
				}
				else
					first = false;
			}
			if (faceBlder.IsDone())
			{
				pFace = new TopoDS_Face();
				*pFace = faceBlder.Face();
			}
			else
				XbimGeometryCreator::LogInfo(logger, compProfile, "Profile could not be built.It has been omitted");

		}


		//Builds a face from a CircleProfileDef
		void XbimFace::Init(IIfcCircleHollowProfileDef ^ circProfile, ILogger^ logger)
		{
			if (circProfile->Radius <= 0)
			{
				XbimGeometryCreator::LogInfo(logger, circProfile, "Circular profile has a radius <= 0. Face discarded");
				return;
			}
			gp_Ax2 gpax2;
			if (circProfile->Position != nullptr)
			{
				IIfcAxis2Placement2D^ ax2 = (IIfcAxis2Placement2D^)circProfile->Position;
				gpax2.SetLocation(gp_Pnt(ax2->Location->X, ax2->Location->Y, 0));
				gpax2.SetDirection(gp_Dir(0, 0, 1));
				gpax2.SetXDirection(gp_Dir(ax2->P[0].X, ax2->P[0].Y, 0.));
			}

			//make the outer wire
			gp_Circ outer(gpax2, circProfile->Radius);
			Handle(Geom_Circle) hOuter = GC_MakeCircle(outer);
			TopoDS_Edge outerEdge = BRepBuilderAPI_MakeEdge(hOuter);
			BRepBuilderAPI_MakeWire outerWire;
			outerWire.Add(outerEdge);
			double innerRadius = circProfile->Radius - circProfile->WallThickness;
			BRepBuilderAPI_MakeFace faceBlder(outerWire);
			//now add inner wire
			if (innerRadius > 0)
			{
				gp_Circ inner(gpax2, circProfile->Radius - circProfile->WallThickness);
				Handle(Geom_Circle) hInner = GC_MakeCircle(inner);
				TopoDS_Edge innerEdge = BRepBuilderAPI_MakeEdge(hInner);
				BRepBuilderAPI_MakeWire innerWire;
				innerEdge.Reverse();
				innerWire.Add(innerEdge);

				faceBlder.Add(innerWire);
			}
			pFace = new TopoDS_Face();
			*pFace = faceBlder.Face();
		}


		void XbimFace::Init(IIfcRectangleHollowProfileDef^ rectProfile, ILogger^ logger)
		{
			if (rectProfile->XDim <= 0 || rectProfile->YDim <= 0)
			{
				XbimGeometryCreator::LogInfo(logger, rectProfile, "Profile has a dimension <= 0,  XDim = {0}, YDim = {1}. Face ignored", rectProfile->XDim, rectProfile->YDim);
			}
			else
			{
				double xOff = rectProfile->XDim / 2;
				double yOff = rectProfile->YDim / 2;
				double precision = rectProfile->Model->ModelFactors->Precision;
				gp_Pnt bl(-xOff, -yOff, 0);
				gp_Pnt br(xOff, -yOff, 0);
				gp_Pnt tr(xOff, yOff, 0);
				gp_Pnt tl(-xOff, yOff, 0);
				//make the vertices
				BRep_Builder builder;
				TopoDS_Vertex vbl, vbr, vtr, vtl;
				builder.MakeVertex(vbl, bl, precision);
				builder.MakeVertex(vbr, br, precision);
				builder.MakeVertex(vtr, tr, precision);
				builder.MakeVertex(vtl, tl, precision);
				//make the edges
				TopoDS_Wire wire;
				builder.MakeWire(wire);
				builder.Add(wire, BRepBuilderAPI_MakeEdge(vbl, vbr));
				builder.Add(wire, BRepBuilderAPI_MakeEdge(vbr, vtr));
				builder.Add(wire, BRepBuilderAPI_MakeEdge(vtr, vtl));
				builder.Add(wire, BRepBuilderAPI_MakeEdge(vtl, vbl));
				wire.Closed(Standard_True);
				double oRad = rectProfile->OuterFilletRadius.HasValue ? (double)(rectProfile->OuterFilletRadius.Value) : 0.0;
				if (oRad > 0) //consider fillets
				{
					BRepBuilderAPI_MakeFace faceMaker(wire, true);
					BRepFilletAPI_MakeFillet2d filleter(faceMaker.Face());
					for (BRepTools_WireExplorer exp(wire); exp.More(); exp.Next())
					{
						filleter.AddFillet(exp.CurrentVertex(), oRad);
					}
					filleter.Build();
					if (filleter.IsDone())
					{
						TopoDS_Shape shape = filleter.Shape();
						for (TopExp_Explorer exp(shape, TopAbs_WIRE); exp.More();) //just take the first wire
						{
							wire = TopoDS::Wire(exp.Current());
							break;
						}
					}
				}
				//make the face
				BRepBuilderAPI_MakeFace faceBlder(wire);
				//calculate hole
				if (rectProfile->WallThickness <= 0)
					XbimGeometryCreator::LogInfo(logger, rectProfile, "Wall thickness of a rectangle hollow profile must be greater than 0, a solid rectangular profile has been used.");
				else
				{
					TopoDS_Wire innerWire;
					builder.MakeWire(innerWire);
					double t = rectProfile->WallThickness;
					gp_Pnt ibl(-xOff + t, -yOff + t, 0);
					gp_Pnt ibr(xOff - t, -yOff + t, 0);
					gp_Pnt itr(xOff - t, yOff - t, 0);
					gp_Pnt itl(-xOff + t, yOff - t, 0);
					TopoDS_Vertex vibl, vibr, vitr, vitl;
					builder.MakeVertex(vibl, ibl, precision);
					builder.MakeVertex(vibr, ibr, precision);
					builder.MakeVertex(vitr, itr, precision);
					builder.MakeVertex(vitl, itl, precision);
					builder.Add(innerWire, BRepBuilderAPI_MakeEdge(vibl, vibr));
					builder.Add(innerWire, BRepBuilderAPI_MakeEdge(vibr, vitr));
					builder.Add(innerWire, BRepBuilderAPI_MakeEdge(vitr, vitl));
					builder.Add(innerWire, BRepBuilderAPI_MakeEdge(vitl, vibl));

					double iRad = rectProfile->InnerFilletRadius.HasValue ? (double)(rectProfile->InnerFilletRadius.Value) : 0.0;
					if (iRad > 0) //consider fillets
					{
						BRepBuilderAPI_MakeFace faceMaker(innerWire, true);
						BRepFilletAPI_MakeFillet2d filleter(faceMaker.Face());
						for (BRepTools_WireExplorer exp(innerWire); exp.More(); exp.Next())
						{
							filleter.AddFillet(exp.CurrentVertex(), iRad);
						}
						filleter.Build();
						if (filleter.IsDone())
						{
							TopoDS_Shape shape = filleter.Shape();
							for (TopExp_Explorer exp(shape, TopAbs_WIRE); exp.More();) //just take the first wire
							{
								innerWire = TopoDS::Wire(exp.Current());
								break;
							}
						}
					}
					innerWire.Reverse();
					innerWire.Closed(Standard_True);
					faceBlder.Add(innerWire);
				}

				pFace = new TopoDS_Face();
				*pFace = faceBlder.Face();
				//apply the position transformation
				if (rectProfile->Position != nullptr)
					pFace->Move(XbimConvert::ToLocation(rectProfile->Position));
				ShapeFix_ShapeTolerance fTol;
				fTol.LimitTolerance(*pFace, rectProfile->Model->ModelFactors->Precision);
			}

		}

		//Builds a face from a Surface
		void XbimFace::Init(IIfcSurface ^ surface, ILogger^ logger)
		{
			if (dynamic_cast<IIfcPlane^>(surface))
				return Init((IIfcPlane^)surface, logger);
			else if (dynamic_cast<IIfcSurfaceOfRevolution^>(surface))
				return Init((IIfcSurfaceOfRevolution^)surface, logger);
			else if (dynamic_cast<IIfcSurfaceOfLinearExtrusion^>(surface))
				return Init((IIfcSurfaceOfLinearExtrusion^)surface, logger);
			else if (dynamic_cast<IIfcCurveBoundedPlane^>(surface))
				return Init((IIfcCurveBoundedPlane^)surface, logger);
			else if (dynamic_cast<IIfcRectangularTrimmedSurface^>(surface))
				return Init((IIfcRectangularTrimmedSurface^)surface, logger);
			else if (dynamic_cast<IIfcBSplineSurface^>(surface))
				return Init((IIfcBSplineSurface^)surface, logger);
			else if (dynamic_cast<IIfcCylindricalSurface^>(surface))
				return Init((IIfcCylindricalSurface^)surface, logger);
			else
			{
				Type ^ type = surface->GetType();
				throw(gcnew NotImplementedException(String::Format("XbimFace. BuildFace of type {0} is not implemented", type->Name)));
			}

		}

		void XbimFace::Init(IIfcCylindricalSurface ^ surface, ILogger^ /*logger*/)
		{
			gp_Ax3 ax3 = XbimConvert::ToAx3(surface->Position);
			Handle(Geom_CylindricalSurface)   gcs = new Geom_CylindricalSurface(ax3, surface->Radius);
			//gp_Cylinder cylinder(ax3, surface->Radius);
			BRepBuilderAPI_MakeFace  builder;
			builder.Init(gcs, Standard_False, surface->Model->ModelFactors->Precision);
			pFace = new TopoDS_Face();
			*pFace = builder.Face();
		}


		void XbimFace::Init(IIfcBSplineSurface ^ surface, ILogger^ logger)
		{
			if (dynamic_cast<IIfcBSplineSurfaceWithKnots^>(surface))
				return Init((IIfcBSplineSurfaceWithKnots^)surface, logger);
			Type^ type = surface->GetType();
			throw(gcnew NotImplementedException(String::Format("XbimFace. BuildFace of type {0} is not implemented", type->Name)));
		}

		void XbimFace::Init(IIfcBSplineSurfaceWithKnots ^ surface, ILogger^ logger)
		{
			if (dynamic_cast<IIfcRationalBSplineSurfaceWithKnots^>(surface))
				return Init((IIfcRationalBSplineSurfaceWithKnots^)surface, logger);

			List<List<XbimPoint3D>^>^ ifcControlPoints = surface->ControlPoints;
			if (surface->ControlPoints->Count < 2)
			{
				XbimGeometryCreator::LogWarning(logger, surface, "Incorrect number of poles for Bspline surface, it must be at least 2");
				return;
			}
			TColgp_Array2OfPnt poles(1, (Standard_Integer)surface->UUpper + 1, 1, (Standard_Integer)surface->VUpper + 1);

			for (int u = 0; u <= surface->UUpper; u++)
			{
				List<XbimPoint3D>^ uRow = ifcControlPoints[u];
				for (int v = 0; v <= surface->VUpper; v++)
				{
					XbimPoint3D cp = uRow[v];
					poles.SetValue(u + 1, v + 1, gp_Pnt(cp.X, cp.Y, cp.Z));
				}

			}

			TColStd_Array1OfReal uknots(1, (Standard_Integer)surface->KnotUUpper);
			TColStd_Array1OfReal vknots(1, (Standard_Integer)surface->KnotVUpper);
			TColStd_Array1OfInteger uMultiplicities(1, (Standard_Integer)surface->KnotUUpper);
			TColStd_Array1OfInteger vMultiplicities(1, (Standard_Integer)surface->KnotVUpper);
			int i = 1;
			for each (double knot in surface->UKnots)
			{
				uknots.SetValue(i, knot);
				i++;
			}
			i = 1;
			for each (double knot in surface->VKnots)
			{
				vknots.SetValue(i, knot);
				i++;
			}

			i = 1;
			for each (int multiplicity in surface->UMultiplicities)
			{
				uMultiplicities.SetValue(i, multiplicity);
				i++;
			}

			i = 1;
			for each (int multiplicity in surface->VMultiplicities)
			{
				vMultiplicities.SetValue(i, multiplicity);
				i++;
			}

			Handle(Geom_BSplineSurface) hSurface = new Geom_BSplineSurface(poles, uknots, vknots, uMultiplicities, vMultiplicities, (Standard_Integer)surface->UDegree, (Standard_Integer)surface->VDegree);
			BRepBuilderAPI_MakeFace faceMaker(hSurface, 0.1/*surface->Model->ModelFactors->Precision*/);
			pFace = new TopoDS_Face();
			*pFace = faceMaker.Face();
			/*ShapeFix_ShapeTolerance FTol;
			FTol.SetTolerance(*pFace, bspline->Model->ModelFactors->Precision, TopAbs_VERTEX);*/
		}
		void XbimFace::Init(IIfcRationalBSplineSurfaceWithKnots ^ surface, ILogger^ /*logger*/)
		{
			List<List<XbimPoint3D>^>^ ifcControlPoints = surface->ControlPoints;
			if (surface->ControlPoints->Count < 2) throw gcnew XbimException("Incorrect number of poles for Bspline surface, must be at least 2");
			TColgp_Array2OfPnt poles(1, (Standard_Integer)surface->UUpper + 1, 1, (Standard_Integer)surface->VUpper + 1);

			for (int u = 0; u <= surface->UUpper; u++)
			{
				List<XbimPoint3D>^ uRow = ifcControlPoints[u];
				for (int v = 0; v <= surface->VUpper; v++)
				{
					XbimPoint3D cp = uRow[v];
					poles.SetValue(u + 1, v + 1, gp_Pnt(cp.X, cp.Y, cp.Z));
				}

			}

			List<List<Ifc4::MeasureResource::IfcReal>^>^ ifcWeights = surface->Weights;
			TColStd_Array2OfReal weights(1, (Standard_Integer)surface->UUpper + 1, 1, (Standard_Integer)surface->VUpper + 1);
			for (int u = 0; u <= surface->UUpper; u++)
			{
				List<Ifc4::MeasureResource::IfcReal>^ uRow = ifcWeights[u];
				for (int v = 0; v <= surface->VUpper; v++)
				{
					double r = uRow[v];
					weights.SetValue(u + 1, v + 1, r);
				}
			}

			TColStd_Array1OfReal uknots(1, (Standard_Integer)surface->KnotUUpper);
			TColStd_Array1OfReal vknots(1, (Standard_Integer)surface->KnotVUpper);
			TColStd_Array1OfInteger uMultiplicities(1, (Standard_Integer)surface->KnotUUpper);
			TColStd_Array1OfInteger vMultiplicities(1, (Standard_Integer)surface->KnotVUpper);
			int i = 1;
			for each (double knot in surface->UKnots)
			{
				uknots.SetValue(i, knot);
				i++;
			}
			i = 1;
			for each (double knot in surface->VKnots)
			{
				vknots.SetValue(i, knot);
				i++;
			}

			i = 1;
			for each (int multiplicity in surface->UMultiplicities)
			{
				uMultiplicities.SetValue(i, multiplicity);
				i++;
			}

			i = 1;
			for each (int multiplicity in surface->VMultiplicities)
			{
				vMultiplicities.SetValue(i, multiplicity);
				i++;
			}

			Standard_Integer uDegree = (Standard_Integer)surface->UDegree;
			Standard_Integer vDegree = (Standard_Integer)surface->VDegree;
			Handle(Geom_BSplineSurface) hSurface = new Geom_BSplineSurface(poles, weights, uknots, vknots, uMultiplicities, vMultiplicities, uDegree, vDegree);
			BRepBuilderAPI_MakeFace faceMaker(hSurface, surface->Model->ModelFactors->Precision);
			pFace = new TopoDS_Face();
			*pFace = faceMaker.Face();
		}

		//Builds a face from a Plane
		void XbimFace::Init(IIfcPlane ^ plane, ILogger^ /*logger*/)
		{
			gp_Ax3 ax3 = XbimConvert::ToAx3(plane->Position);
			gp_Pln pln(ax3);
			BRepBuilderAPI_MakeFace  builder(pln);
			pFace = new TopoDS_Face();
			*pFace = builder.Face();
		}
		void XbimFace::Init(IIfcSurfaceOfRevolution ^ sRev, ILogger^ logger)
		{
			XbimWire^ curve = gcnew XbimWire(sRev->SweptCurve, logger);
			if (!curve->IsValid || curve->Edges->Count > 1)
			{
				XbimGeometryCreator::LogWarning(logger, sRev, "Invalid swept curve = #{0} in surface of revolution = #{1}, face discarded", sRev->SweptCurve->EntityLabel);
				return;
			}
			XbimEdge^ edge = (XbimEdge^)curve->Edges->First;
			TopLoc_Location loc;
			Standard_Real start, end;
			Handle(Geom_Curve) c3d = BRep_Tool::Curve(edge, loc, start, end);
			Handle(Geom_TrimmedCurve) trimmed = new Geom_TrimmedCurve(c3d, start, end);
			gp_Pnt origin(sRev->AxisPosition->Location->X, sRev->AxisPosition->Location->Y, sRev->AxisPosition->Location->Z);
			gp_Dir axisDir(0, 0, 1);
			if (sRev->AxisPosition->Axis != nullptr)
				axisDir = gp_Dir(sRev->AxisPosition->Axis->X, sRev->AxisPosition->Axis->Y, sRev->AxisPosition->Axis->Z);
			gp_Ax1 axis(origin, axisDir);

			Handle(Geom_SurfaceOfRevolution) rev = new  Geom_SurfaceOfRevolution(trimmed, axis);
			BRep_Builder aBuilder;
			pFace = new TopoDS_Face();
			Standard_Real precision = sRev->Model->ModelFactors->Precision;
			aBuilder.MakeFace(*pFace, rev, precision);

			////BRepBuilderAPI_MakeFace faceMaker(geomLin, 0, 2* Math::PI, start, end, sRev->Model->ModelFactors->Precision);
			//
			//
			//if (faceMaker.IsDone())
			//{
			//	pFace = new TopoDS_Face();
			//	*pFace = faceMaker.Face();
			//	//apply the position transformation
			//	if (sRev->Position != nullptr)
			//		pFace->Move(XbimConvert::ToLocation(sRev->Position));
			//}
			//else
			//	XbimGeometryCreator::logger->ErrorFormat("WF011: Invalid swept curve = #{0}. Found in IIfcSurfaceOfRevolution = #{1}, face discarded", sRev->SweptCurve->EntityLabel, sRev->EntityLabel);

		}

		void XbimFace::Init(IIfcRectangularTrimmedSurface^ def, ILogger^ logger)
		{
			Init(def->BasisSurface, logger); //initialise the plane
			if (IsValid)
			{
				TopLoc_Location loc;
				Handle(Geom_Surface) surface = BRep_Tool::Surface(this, loc);
				Handle(Geom_RectangularTrimmedSurface) geomTrim(new  Geom_RectangularTrimmedSurface(surface, def->U1, def->U2, def->V1, def->V2));

				BRepBuilderAPI_MakeFace faceMaker(geomTrim, def->Model->ModelFactors->Precision);
				if (faceMaker.IsDone())
				{
					pFace = new TopoDS_Face();
					*pFace = faceMaker.Face();
				}
				else
					XbimGeometryCreator::LogWarning(logger, def, "Invalid trimed surface = #{0} in rectangular trimmed surface. Face discarded", def->BasisSurface->EntityLabel);
			}
		}

		void XbimFace::Init(IIfcCurveBoundedPlane^ def, ILogger^ logger)
		{
			Init(def->BasisSurface, logger); //initialise the plane
			if (IsValid)
			{
				XbimWire^ outerBound = gcnew XbimWire(def->OuterBoundary, logger);
				BRepBuilderAPI_MakeFace  builder(this);
				builder.Add(outerBound);
				for each (IIfcCurve^ innerCurve in def->InnerBoundaries)
				{
					XbimWire^ innerBound = gcnew XbimWire(innerCurve, logger);
					if (innerBound->IsValid)
						builder.Add(innerBound);
					else
						XbimGeometryCreator::LogWarning(logger, def, "Invalid inner bound = #{0} found in curve bounded plane. Inner bound ignored", innerCurve->EntityLabel);

				}
				if (builder.IsDone())
				{
					pFace = new TopoDS_Face();
					*pFace = builder.Face();
				}
				else
					XbimGeometryCreator::LogWarning(logger, def, "Invalid outer bound = #{0} found in curve bounded plane. Face discarded", def->OuterBoundary->EntityLabel);
			}
		}

		void XbimFace::Init(IIfcSurfaceOfLinearExtrusion ^ sLin, ILogger^ logger)
		{


			if (sLin->SweptCurve->ProfileType != IfcProfileTypeEnum::CURVE)
			{
				XbimGeometryCreator::LogWarning(logger, sLin, "Only profiles of type curve are valid in a surface of linearExtrusion {0}. Face discarded", sLin->SweptCurve->EntityLabel);
				return;
			}

			XbimWire^ curve = gcnew XbimWire(sLin->SweptCurve, logger);

			TopoDS_Edge edge = (XbimEdge^)curve->Edges->First;
			TopLoc_Location loc;
			Standard_Real start, end;
			Handle(Geom_Curve) c3d = BRep_Tool::Curve(edge, loc, start, end);

			gp_Vec extrude = XbimConvert::GetDir3d(sLin->ExtrudedDirection);
			extrude.Normalize();
			extrude *= sLin->Depth;
			Handle(Geom_SurfaceOfLinearExtrusion) geomLin(new  Geom_SurfaceOfLinearExtrusion(c3d, extrude));
			BRepBuilderAPI_MakeFace faceMaker(geomLin, sLin->Model->ModelFactors->Precision);
			if (faceMaker.IsDone())
			{
				pFace = new TopoDS_Face();
				*pFace = faceMaker.Face();
				//apply the position transformation unless from a model that has this incorrect
				// versions of the IFC explorter on or before 17.0.416 for Revit duplicated the placement, ignore for a correct result
				if (!sLin->Model->ModelFactors->ApplyWorkAround("#SurfaceOfLinearExtrusion"))
				{
					if (sLin->Position != nullptr)
						pFace->Move(XbimConvert::ToLocation(sLin->Position));
				}
			}
			else
				XbimGeometryCreator::LogWarning(logger, sLin, "Invalid swept curve = #{0} found in surface of linearExtrusion. Face discarded", sLin->SweptCurve->EntityLabel);


		}

		void  XbimFace::Init(double x, double y, double tolerance, ILogger^ /*logger*/)
		{
			XbimWire^ bounds = gcnew XbimWire(x, y, tolerance, false);
			if (bounds->IsValid)
			{
				BRepBuilderAPI_MakeFace faceMaker(bounds);
				pFace = new TopoDS_Face();
				*pFace = faceMaker.Face();
			}
		}

#pragma region IXbimFace Interface

		XbimWire^ XbimFace::OuterWire::get()
		{
			if (pFace == nullptr) return nullptr;
			TopoDS_Wire outerWire = BRepTools::OuterWire(*pFace);//get the outer loop
			GC::KeepAlive(this);
			if (outerWire.IsNull()) //check if this is a half space
			{
				return nullptr;
			}
			else
			{
				return gcnew XbimWire(outerWire);
			}

		}

		XbimWireSet^ XbimFace::Wires::get()
		{
			if (!IsValid) return XbimWireSet::Empty; //return an empty list, avoid using Enumberable::Empty to avoid LINQ dependencies			
			return gcnew XbimWireSet(this);
		}

		XbimWireSet^ XbimFace::InnerWires::get()
		{
			if (!IsValid) return XbimWireSet::Empty; //return an empty list, avoid using Enumberable::Empty to avoid LINQ dependencies
			TopoDS_Wire outerWire = BRepTools::OuterWire(*pFace);//get the outer loop
			if (outerWire.IsNull()) return XbimWireSet::Empty;//check if this is a half space
			TopTools_ListOfShape wires;
			for (TopExp_Explorer wireEx(*pFace, TopAbs_WIRE); wireEx.More(); wireEx.Next())
			{
				if (!wireEx.Current().IsEqual(outerWire))
					wires.Append(TopoDS::Wire(wireEx.Current()));
			}
			GC::KeepAlive(this);
			return gcnew XbimWireSet(wires);
		}


		XbimRect3D XbimFace::BoundingBox::get()
		{
			if (pFace == nullptr) return XbimRect3D::Empty;
			Bnd_Box pBox;
			BRepBndLib::Add(*pFace, pBox);
			Standard_Real srXmin, srYmin, srZmin, srXmax, srYmax, srZmax;
			if (pBox.IsVoid()) return XbimRect3D::Empty;
			pBox.Get(srXmin, srYmin, srZmin, srXmax, srYmax, srZmax);
			GC::KeepAlive(this);
			return XbimRect3D(srXmin, srYmin, srZmin, (srXmax - srXmin), (srYmax - srYmin), (srZmax - srZmin));
		}

		IXbimGeometryObject^ XbimFace::Transform(XbimMatrix3D matrix3D)
		{
			BRepBuilderAPI_Copy copier(this);
			BRepBuilderAPI_Transform gTran(copier.Shape(), XbimConvert::ToTransform(matrix3D));
			TopoDS_Face temp = TopoDS::Face(gTran.Shape());
			GC::KeepAlive(this);
			return gcnew XbimFace(temp);
		}

		IXbimGeometryObject^ XbimFace::TransformShallow(XbimMatrix3D matrix3D)
		{
			TopoDS_Face face = TopoDS::Face(pFace->Moved(XbimConvert::ToTransform(matrix3D)));
			GC::KeepAlive(this);
			return gcnew XbimFace(face);
		}

		bool XbimFace::IsQuadOrTriangle::get()
		{
			if (!IsValid) return false;
			if (!IsPlanar) return false;
			TopExp_Explorer wireEx(*pFace, TopAbs_WIRE);
			wireEx.Next();
			if (wireEx.More()) return false; //it has holes
			TopTools_IndexedMapOfShape map;
			TopExp::MapShapes(*pFace, TopAbs_VERTEX, map);
			GC::KeepAlive(this);
			if (map.Extent() == 4 || map.Extent() == 3) return true;
			return false;
		}


		double XbimFace::Area::get()
		{
			if (IsValid)
			{
				GProp_GProps gProps;
				BRepGProp::SurfaceProperties(*pFace, gProps);
				GC::KeepAlive(this);
				return gProps.Mass();
			}
			else
				return 0;
		}

		double XbimFace::Perimeter::get()
		{
			if (IsValid)
			{
				GProp_GProps gProps;
				BRepGProp::LinearProperties(*pFace, gProps);
				GC::KeepAlive(this);
				return gProps.Mass();
			}
			else
				return 0;

		}

		XbimVector3D XbimFace::Normal::get()
		{
			if (!IsValid) return XbimVector3D();
			TopoDS_Face face = this;
			BRepGProp_Face prop(face);
			gp_Pnt centre;
			gp_Vec normalDir;
			double u1, u2, v1, v2;
			prop.Bounds(u1, u2, v1, v2);
			prop.Normal((u1 + u2) / 2.0, (v1 + v2) / 2.0, centre, normalDir);
			XbimVector3D vec(normalDir.X(), normalDir.Y(), normalDir.Z());
			GC::KeepAlive(this);
			return vec.Normalized();
		}

		XbimVector3D XbimFace::NormalAt(double u, double v)
		{
			if (!IsValid) return XbimVector3D();
			TopoDS_Face face = this;
			BRepGProp_Face prop(face);
			gp_Pnt pos;
			gp_Vec normalDir;
			double u1, u2, v1, v2;
			prop.Bounds(u1, u2, v1, v2);
			prop.Normal(u, v, pos, normalDir);
			XbimVector3D vec(normalDir.X(), normalDir.Y(), normalDir.Z());
			return vec.Normalized();

		}
		XbimPoint3D XbimFace::Location::get()
		{
			if (!IsValid) return XbimPoint3D();
			BRepGProp_Face prop(*pFace);
			gp_Pnt centre;
			gp_Vec normalDir;
			double u1, u2, v1, v2;
			prop.Bounds(u1, u2, v1, v2);
			prop.Normal((u1 + u2) / 2.0, (v1 + v2) / 2.0, centre, normalDir);
			XbimPoint3D loc(centre.X(), centre.Y(), centre.Z());
			GC::KeepAlive(this);
			return loc;
		}

		bool XbimFace::IsPlanar::get()
		{
			if (!IsValid) return false;
			Handle(Geom_Surface) surf = BRep_Tool::Surface(*pFace); //the surface
			Standard_Real tol = BRep_Tool::Tolerance(*pFace);
			GeomLib_IsPlanarSurface ps(surf, tol);
			GC::KeepAlive(this);
			return ps.IsPlanar() == Standard_True; //see if we have a planar or curved surface, if planar we need only worry about one normal

		}



		bool XbimFace::IsPolygonal::get()
		{
			if (!IsValid) return false;
			TopTools_IndexedMapOfShape map;
			TopExp::MapShapes(*pFace, TopAbs_EDGE, map);
			for (Standard_Integer i = 1; i <= map.Extent(); i++)
			{
				Standard_Real start, end;
				Handle(Geom_Curve) c3d = BRep_Tool::Curve(TopoDS::Edge(map(i)), start, end);
				if (!c3d.IsNull())
				{
					Handle(Standard_Type) cType = c3d->DynamicType();
					if (cType != STANDARD_TYPE(Geom_Line))
					{
						if (cType != STANDARD_TYPE(Geom_TrimmedCurve)) return false;
						Handle(Geom_TrimmedCurve) tc = Handle(Geom_TrimmedCurve)::DownCast(c3d);
						Handle(Standard_Type) tcType = tc->DynamicType();
						if (tcType != STANDARD_TYPE(Geom_Line)) return false;
					}
				}
			}
			GC::KeepAlive(this);
			return true;
		}

#pragma endregion

#pragma region Equality Overrides

		bool XbimFace::Equals(Object^ obj)
		{
			XbimFace^ f = dynamic_cast<XbimFace^>(obj);
			if (f == nullptr) return false;
			return this == f;
		}

		bool XbimFace::Equals(IXbimFace^ obj)
		{
			XbimFace^ f = dynamic_cast<XbimFace^>(obj);
			if (f == nullptr) return false;
			return this == f;
		}

		int XbimFace::GetHashCode()
		{
			if (!IsValid) return 0;
			return pFace->HashCode(Int32::MaxValue);
		}

		bool XbimFace::operator ==(XbimFace^ left, XbimFace^ right)
		{
			// If both are null, or both are same instance, return true.
			if (System::Object::ReferenceEquals(left, right))
				return true;

			// If one is null, but not both, return false.
			if (((Object^)left == nullptr) || ((Object^)right == nullptr))
				return false;
			return  ((const TopoDS_Face&)left).IsEqual(right) == Standard_True;

		}

		bool XbimFace::operator !=(XbimFace^ left, XbimFace^ right)
		{
			return !(left == right);
		}

		void XbimFace::SaveAsBrep(String^ fileName)
		{
			if (IsValid)
			{
				XbimOccWriter^ occWriter = gcnew XbimOccWriter();
				occWriter->Write(this, fileName);
			}
		}

#pragma endregion


#pragma region Methods

		void XbimFace::Move(IIfcAxis2Placement3D^ position)
		{
			if (!IsValid) return;
			gp_Trsf toPos = XbimConvert::ToTransform(position);
			pFace->Move(toPos);
		}

		void XbimFace::Move(gp_Trsf transform)
		{
			if (!IsValid) return;
			pFace->Move(transform);
		}

		void XbimFace::Translate(XbimVector3D translation)
		{
			if (!IsValid) return;
			gp_Vec v(translation.X, translation.Y, translation.Z);
			gp_Trsf t;
			t.SetTranslation(v);
			pFace->Move(t);
		}

		void XbimFace::Reverse()
		{
			if (!IsValid) return;
			pFace->Reverse();
		}

		bool XbimFace::Add(IXbimWire^ innerWire)
		{
			if (!IsValid) return false;
			XbimWire^ wire = dynamic_cast<XbimWire^>(innerWire);
			if (wire == nullptr) throw gcnew ArgumentException("Only IXbimWires created bu Xbim.OCC modules are supported", "xbimWire");
			BRepBuilderAPI_MakeFace faceMaker(*pFace);
			faceMaker.Add(wire);
			if (!faceMaker.IsDone()) return false;
			*pFace = faceMaker.Face();
			GC::KeepAlive(this);
			return true;
		}


		XbimPoint3D XbimFace::PointAtParameters(double u, double v)
		{
			if (!IsValid) return XbimPoint3D();
			const Handle(Geom_Surface) &surface = BRep_Tool::Surface(*pFace);
			gp_Pnt p;
			surface->D0(u, v, p);
			GC::KeepAlive(this);
			return XbimPoint3D(p.X(), p.Y(), p.Z());
		}

		Handle(Geom_Surface) XbimFace::GetSurface()
		{
			TopoDS_Face face = this;
			return BRep_Tool::Surface(face);
		}

		void XbimFace::SetLocation(TopLoc_Location loc)
		{
			if (IsValid)
				pFace->Move(loc);
		}

		XbimGeometryObject ^ XbimFace::Transformed(IIfcCartesianTransformationOperator ^ transformation)
		{
			IIfcCartesianTransformationOperator3DnonUniform^ nonUniform = dynamic_cast<IIfcCartesianTransformationOperator3DnonUniform^>(transformation);
			if (nonUniform != nullptr)
			{
				gp_GTrsf trans = XbimConvert::ToTransform(nonUniform);
				BRepBuilderAPI_GTransform tr(this, trans, Standard_True); //make a copy of underlying shape
				GC::KeepAlive(this);
				return gcnew XbimFace(TopoDS::Face(tr.Shape()), Tag);
			}
			else
			{
				gp_Trsf trans = XbimConvert::ToTransform(transformation);
				BRepBuilderAPI_Transform tr(this, trans, Standard_False); //do not make a copy of underlying shape
				GC::KeepAlive(this);
				return gcnew XbimFace(TopoDS::Face(tr.Shape()), Tag);
			}
		}

		XbimGeometryObject ^ XbimFace::Moved(IIfcPlacement ^ placement)
		{
			if (!IsValid) return this;
			XbimFace^ copy = gcnew XbimFace(*pFace, Tag); //take a copy of the shape
			TopLoc_Location loc = XbimConvert::ToLocation(placement);
			copy->Move(loc);
			return copy;
		}

		XbimGeometryObject ^ XbimFace::Moved(IIfcObjectPlacement ^ objectPlacement, ILogger^ logger)
		{
			if (!IsValid) return this;
			XbimFace^ copy = gcnew XbimFace(*pFace, Tag); //take a copy of the shape
			TopLoc_Location loc = XbimConvert::ToLocation(objectPlacement, logger);
			copy->Move(loc);
			return copy;
		}

#pragma endregion

	}
}