#include <cmath>
#include <memory>
#include "EBCCProjector.H"
#include "Chombo_NamespaceHeader.H"
/// 
EBCCProjector::
EBCCProjector(shared_ptr<EBEncyclopedia<2, Real> >   & a_brit,     
              shared_ptr<GeometryService<2> >        & a_geoserv,  
              const DisjointBoxLayout                & a_grids,    
              const Box                              & a_domain,   
              const Real                             & a_dx,       
              const IntVect                          & a_nghost)   
{
  m_macprojector = shared_ptr<EBMACProjector>
    (new EBMACProjector(a_brit,     
                        a_geoserv,  
                        a_grids,    
                        a_domain,   
                        a_dx,       
                        a_nghost));
  registerStencils();
}
////
void  
EBCCProjector::
registerStencils()
{
  auto & brit    = m_macprojector->m_brit;
  auto & doma    = m_macprojector->m_domain;
  for(int idir = 0; idir < DIM; idir++)
  {
    //dirichlet at domain to get zero normal velocity at domain boundaries
    //grown by one to allow interpolation to face centroids
    brit->registerCellToFace(StencilNames::AveCellToFace, StencilNames::Dirichlet, StencilNames::Neumann, doma, doma, false, Point::Ones());
    brit->registerFaceToCell(StencilNames::AveFaceToCell, StencilNames::NoBC     , StencilNames::NoBC   , doma, doma, false);
  }

}
/// 
void 
EBCCProjector::
project(EBLevelBoxData<CELL, DIM>   & a_velo,
        EBLevelBoxData<CELL, DIM>   & a_gphi,
        Real a_tol, unsigned int a_maxiter)
{
  auto & rhs     = m_macprojector->m_rhs;
  auto & brit    = m_macprojector->m_brit;
  auto & phi     = m_macprojector->m_phi;
  auto & doma    = m_macprojector->m_domain;
  auto & solver  = m_macprojector->m_solver;
  auto & grids   = m_macprojector->m_grids;
  auto & graphs  = m_macprojector->m_graphs;
  auto & nghost  = m_macprojector->m_nghost;

  // set rhs = kappa*div (vel)
  kappaDivU(rhs, a_velo);

  //solve kappa*lapl(phi) = kappa*div(vel)
  solver->solve(phi, rhs, a_tol, a_maxiter);
  
  //v := v - gphi
  DataIterator dit = grids.dataIterator();
  int ideb = 0;
  for(int ibox = 0; ibox < dit.size(); ++ibox)
  {
    const EBGraph  & graph = (*graphs)[dit[ibox]];
    Bx   grid   =  ProtoCh::getProtoBox(grids[dit[ibox]]);
    Bx  grown   =  grid.grow(ProtoCh::getPoint(nghost));

    //get face fluxes and interpolate them to centroids
    EBFluxData<Real, 1>         facegrad(grown, graph);
    //gphi = grad(phi)
    for(unsigned int idir = 0; idir < DIM; idir++)
    {
      bool initZero = true;
      //registered by the mac projector
      brit->applyCellToFace(StencilNames::MACGradient, StencilNames::NoBC, doma,
                            facegrad, phi[dit[ibox]], idir, ibox, initZero, 1.0);

      EBBoxData<CELL, Real, 1> gradComp;
      gradComp.define(a_gphi[dit[ibox]], idir);
      brit->applyFaceToCell(StencilNames::AveFaceToCell, StencilNames::NoBC, doma,
                            gradComp, facegrad,  idir, ibox, initZero, 1.0);
      
    }
    a_velo[dit[ibox]] -= a_gphi[dit[ibox]];
    ideb++;
  }
}
///
void 
EBCCProjector::
kappaDivU(EBLevelBoxData<CELL, 1  > & a_divu,
          EBLevelBoxData<CELL, DIM> & a_velo)
{
  auto & brit    = m_macprojector->m_brit;
  auto & doma    = m_macprojector->m_domain;
  auto & divu    = m_macprojector->m_rhs;
  auto & grids   = m_macprojector->m_grids;
  auto & graphs  = m_macprojector->m_graphs;
  auto & nghost  = m_macprojector->m_nghost;
  auto & kappa   = m_macprojector->m_solver->getKappa();

  DataIterator dit = grids.dataIterator();
  int ideb = 0;
  for(int ibox = 0; ibox < dit.size(); ++ibox)
  {

    Bx   grid   =  ProtoCh::getProtoBox(grids[dit[ibox]]);
    const EBGraph  & graph = (*graphs)[dit[ibox]];
    Bx  grown   =  grid.grow(ProtoCh::getPoint(nghost));

    //get face fluxes and interpolate them to centroids
    EBFluxData<Real, 1>         centroidv(grown, graph);
    EBFluxData<Real, 1>         facecentv(grown, graph);

    EBBoxData<CELL, Real, DIM>& inputvelo =  a_velo[dit[ibox]];
    //this one was registered by the mac projector
    EBFluxStencil<2, Real> interpsten  = brit->getFluxStencil(StencilNames::InterpToFaceCentroid, StencilNames::NoBC   , doma, doma, ibox);
    bool initZero = true;
    for(unsigned int idir = 0; idir < DIM; idir++)
    {
      EBBoxData<CELL, Real, 1> velcomp;
      velcomp.define(inputvelo, idir);
      brit->applyCellToFace(StencilNames::AveCellToFace, StencilNames::Neumann, 
                            doma,facecentv, velcomp, idir, ibox,  initZero, 1.0);
    }

//standard algorithm does not do this
    interpsten.apply(centroidv, facecentv, initZero, 1.0);  


    auto& kapdiv = divu[dit[ibox]];
    bool initToZero = false;
    //also registered by the mac projector
    brit->applyFaceToCell(StencilNames::DivergeFtoC, StencilNames::NoBC, doma, kapdiv, centroidv,
                          ibox, initToZero, 1.0);
    ideb++;
  }

  static bool printed = false;
  if(!printed)
  {
    printed = true;
    writeEBLevelHDF5(string("divuinitc4.hdf5"), divu, kappa, doma, graphs);
  }
}
///
#include "Chombo_NamespaceFooter.H"


