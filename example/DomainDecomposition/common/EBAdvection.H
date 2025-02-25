#ifndef _EBAdvection_H_
#define _EBAdvection_H_
#include <cmath>
#include <memory>
#include "Proto.H"
#include "Chombo_EBDictionary.H"
#include "Chombo_EBChombo.H"
#include "Chombo_EBEncyclopedia.H"
#include "Chombo_EBLevelBoxData.H"
#include "EBIBC.H"
#include "Chombo_NamespaceHeader.H"

///class to advect scalars in an eb context (via Trebotich et al.)
class EBAdvection
{
public:
  /// 
  EBAdvection(shared_ptr<EBEncyclopedia<2, Real> >   & a_brit,
              shared_ptr<GeometryService<2> >        & a_geoserv,
              shared_ptr<EBLevelBoxData<CELL, DIM> > & a_veloCell,
              const DisjointBoxLayout                & a_grids,
              const Box                              & a_domain,
              const Real                             & a_dx,
              const EBIBC                            & a_ebibc,
              const IntVect                          & a_nghost);

  virtual ~EBAdvection()
  {
  }

  /// advance one time step (via Trebotich et al.) in  an eb context
  void 
  advance(EBLevelBoxData<CELL, 1>   & a_scal,
          const  Real               & a_dt,
          Real a_inflowVal = 0);

  const EBLevelBoxData<CELL, 1> &
  getKappa() const
  {
    return m_kappa;
  }

  virtual void  kappaConsDiv(EBLevelBoxData<CELL, 1>   & a_scal,
                             const Real                & a_dt,
                             Real inflowVal = 0);
  ///
  /**
     Fills m_hybridDiv (public member) with hybrid divergence.
     A strange user interface, I grant you.  I did not think I would need this back door
  */
  virtual void 
  hybridDivergence(EBLevelBoxData<CELL, 1>   & a_scal,
                   const  Real               & a_dt,
                   Real inflowVal = 0);


  EBLevelBoxData<CELL, 1>                m_hybridDiv;
  EBLevelBoxData<CELL, 1>                m_kappa;

  //additional terms added to extrapolation
  EBLevelBoxData<CELL, 1>                m_source;
private:  
  virtual void applyScalarFluxBCs(EBFluxData<Real, 1> & a_flux,
                                  const DataIndex     & a_dit,
                                  Real inflowVal = 0) const;
   
protected:
  
  //rule britannica
  shared_ptr<EBEncyclopedia<2, Real> >   m_brit;
  DisjointBoxLayout                      m_grids;
  Box                                    m_domain;
  IntVect                                m_nghost;
  shared_ptr<EBLevelBoxData<CELL, DIM> > m_veloCell;
  Real m_dx;
  EBLevelBoxData<CELL, 1>                m_deltaM;
  EBLevelBoxData<CELL, 1>                m_nonConsDiv;
  EBLevelBoxData<CELL, 1>                m_kappaDiv;
  shared_ptr<LevelData<EBGraph>  >       m_graphs;
  Copier                                 m_exchangeCopier;
  EBIBC                                  m_ebibc;
  static const string s_ncdivLabel;      //for non-conservative divergence
  static const string s_nobcsLabel;      //none of the operators here have eb boundary conditions
  static const string s_redistLabel;     //for redistribution
  static const string s_centInterpLabel; //for interpolation to face centroids
  static const string s_slopeLowLabel;   //for cell centered slopes
  static const string s_slopeHighLabel;  //for cell centered slopes
  static const string s_aveCToFLabel;    //get the velocity from cell centers to faces
  static const string s_diriLabel;       //label for dirichlet bcs
  static const string s_neumLabel;       //label for neumann bcs
  static const string s_extrLabel;       //label for extrapolation bcs
  static const string s_divergeLabel;    //label incrementing  the divergence with face fluxes
  static const string s_CtoFLowLabel;    //for getting stuff from low  side cells to faces
  static const string s_CtoFHighLabel;   //for getting stuff from high side cells to faces

  template<CENTERING cent>
  inline void
  getFaceVelComp(EBBoxData<cent, Real, 1>                       & a_fcvel,
                 shared_ptr< EBDictionary<2, Real, CELL, cent> >& a_dict,
                 EBBoxData<CELL, Real, DIM>                     & a_velcc,
                 unsigned int                                     a_idir,
                 int                                              a_ibox)
  {
    EBBoxData<CELL, Real, 1> cellcomp;
    cellcomp.define<DIM>(a_velcc, a_idir);
    // stencils to average velocities from cells to faces
    const auto& facesten = a_dict->getEBStencil(s_aveCToFLabel, s_nobcsLabel, m_domain, m_domain, a_ibox);
    bool initToZero = true;
    facesten->apply(a_fcvel, cellcomp, initToZero, 1.0);               
  }

  void getFaceCenteredVel(EBFluxData<Real, 1>& a_fcvel,
                          const DataIndex    & a_dit,
                          const int          & a_ibox);

  void
  getFaceCenteredFlux(EBFluxData<Real, 1>      & a_fcflux,
                      EBFluxData<Real, 1>      & a_fcvel,
                      EBBoxData<CELL, Real, 1> & a_scal,
                      const DataIndex          & a_dit,
                      int                        a_ibox,
                      Real                       a_dt);

  void
  getUpwindState(EBFluxData<Real, 1>&  a_upwindScal,
                 EBFluxData<Real, 1>&  a_faceCentVelo,
                 EBFluxData<Real, 1>&  a_scalLo,
                 EBFluxData<Real, 1>&  a_scalHi,
                 unsigned int a_curcomp, //for vel advection
                 unsigned int doingvel); // 0 or 1

  void assembleFlux(EBFluxData<Real, 1>& a_fcflux,
                    EBFluxData<Real, 1>& a_scalar,
                    EBFluxData<Real, 1>& a_fcvel);

  void  defineData(shared_ptr<GeometryService<2> >        & a_geoserv);
  void  nonConsDiv();
  void redistribute(EBLevelBoxData<CELL, 1>& a_hybridDiv);
  void fillKappa(shared_ptr<GeometryService<2> >        & a_geoserv);
  void registerStencils();

  void
  bcgExtrapolateScalar(EBFluxData<Real, 1>            & a_scalLo,
                       EBFluxData<Real, 1>            & a_scalHi,
                       EBBoxData<CELL, Real, DIM>     & a_veccell,
                       const EBBoxData<CELL, Real, 1> & a_scal,
                       const EBBoxData<CELL, Real, 1> & a_source,
                       const Bx                       & a_grown,
                       const EBGraph                  & a_graph,
                       const DataIndex                & a_dit,
                       const int                      & a_ibox,
                       const Real                     & a_dt);
  //do the linear combination of kappa div and nc div to get hybrid.  also fills delta m
  void kappaDivPlusOneMinKapDivNC(EBLevelBoxData<CELL, 1>       & a_hybridDiv);

  void getKapDivFFromCentroidFlux(EBBoxData<CELL, Real, 1> &  a_kapdiv,
                                  EBFluxData<Real, 1>      &  a_centroidFlux,
                                  unsigned int a_ibox);

  //disallowed for all the usual reasons
  EBAdvection();
  void operator=(const EBAdvection& a_opin);

};
#include "Chombo_NamespaceFooter.H"

#endif
