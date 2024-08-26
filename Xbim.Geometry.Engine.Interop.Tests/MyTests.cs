using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Linq;
using Xbim.Ifc;
using Xbim.ModelGeometry.Scene;


namespace Xbim.Geometry.Engine.Interop.Tests
{
    [TestClass]
    public class MyTests
    {
        [TestMethod]
        public void Ifc4_MyTest_ARCHI_COMBINED()
        {
            using (var m = IfcStore.Open(@"C:\Dev\LT\IFC_Issues\ARCHI COMBINED.ifc"))
            {
                //m.Instances.Count == 150991;

                var context = new Xbim3DModelContext(m);
                context.CreateContext();

                var numShapes = context.ShapeGeometries().ToArray().Length;
                //numShapes.Should().NotBe(0);

                m.SaveAs(@"C:\Dev\LT\IFC_Issues\Tests\ARCHI_COMBINED_TEST.ifc");

            }
        }

        [TestMethod]
        public void Ifc4_MyTest_Architectural()
        {
            using (var m = IfcStore.Open(@"C:\Dev\LT\IFC_Issues\Architectural model (1).ifc"))
            {
                //m.Instances.Count.Should().Be(167626);

                var context = new Xbim3DModelContext(m);
                context.CreateContext();

                var numShapes = context.ShapeGeometries().ToArray().Length;
                //numShapes.Should().NotBe(0);

                m.SaveAs(@"C:\Dev\LT\IFC_Issues\Tests\Architectural model (1)_TEST.ifc");
            }
        }

        [TestMethod]
        public void Ifc2x3_MyTest_1_Gridline()
        {
            using (var m = IfcStore.Open(@"C:\Dev\LT\IFC_Issues\1_Gridline test model.ifc"))
            {
                //m.Instances.Count.Should().Be(28315);

                var context = new Xbim3DModelContext(m);
                context.CreateContext();

                var numShapes = context.ShapeGeometries().ToArray().Length;
                //numShapes.Should().NotBe(0);

                m.SaveAs(@"C:\Dev\LT\IFC_Issues\Tests\1_Gridline test model_TEST.ifc");

            }
        }

        [TestMethod]
        public void Ifc2x3_MyTest_2_Gridline()
        {
            using (var m = IfcStore.Open(@"C:\Dev\LT\IFC_Issues\2_Gridline test model.ifc"))
            {
               // m.Instances.Count.Should().Be(28316);

                var context = new Xbim3DModelContext(m);
                context.CreateContext();

                var numShapes = context.ShapeGeometries().ToArray().Length;
                //numShapes.Should().NotBe(0);

                m.SaveAs(@"C:\Dev\LT\IFC_Issues\Tests\2_Gridline test model_TEST.ifc");
            }
        }

        [TestMethod]
        public void Ifc2x3_MyTest_3_Gridline()
        {
            using (var m = IfcStore.Open(@"C:\Dev\LT\IFC_Issues\3_Gridline test model.ifc"))
            {
                //m.Instances.Count.Should().Be(28315);

                var context = new Xbim3DModelContext(m);
                context.CreateContext();

                var numShapes = context.ShapeGeometries().ToArray().Length;
                //numShapes.Should().NotBe(0);

                m.SaveAs(@"C:\Dev\LT\IFC_Issues\Tests\3_Gridline test model_TEST.ifc");
            }
        }
    }
}
