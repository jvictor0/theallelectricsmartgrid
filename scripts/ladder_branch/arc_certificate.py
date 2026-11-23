"""Certify a connected curve through folds using moving hyperplanes at sample zero."""
import argparse
import json
from pathlib import Path
import numpy as np

from resonance_chain import Chain
from phase_tube import Ball, EPS, TINY, Jet, advance, constant, hull, stack
from pilot import ACTIVE, LIB, Tone

class Arc(Chain):
    def point(self,left,right,normal):
        proposal=(left[0]+right[0])/2
        z=proposal.copy()
        ix=np.r_[ACTIVE,14]
        for _ in range(18):
            k=z[-1]
            self.tone.set_resonance(np.log1p(48*k/3.9)/np.log(49))
            t=self.tone
            scale=self.scale[:-1]
            result=(t.period(z[:-1])-z[:-1])[ACTIVE]/scale[ACTIVE]
            gauge=((z-proposal)/self.scale)@normal
            if max(abs(result))<1e-10 and abs(gauge)<1e-10: break
            jac=t.double_jacobian(z[:-1],scale[ACTIVE])-np.eye(9)
            h=2e-6
            t.set_resonance(np.log1p(48*(k+h)/3.9)/np.log(49)); plus=t.period(z[:-1])
            t.set_resonance(np.log1p(48*(k-h)/3.9)/np.log(49)); minus=t.period(z[:-1])
            a=np.column_stack((jac,(plus-minus)[ACTIVE]/(2*h*scale[ACTIVE])))
            delta=np.linalg.solve(np.vstack((a,normal[ix])),np.r_[result,gauge])
            z[ix]-=self.scale[ix]*delta
        else:
            raise ArithmeticError("midpoint shooting failed")
        return self.states(z)

    def states(self,z):
        t=self.tone
        t.set_resonance(np.log1p(48*z[-1]/3.9)/np.log(49))
        state=z[:-1].copy()
        states=[]
        for i in range(t.q):
            states.append(np.r_[state,z[-1]])
            LIB.ReturnMap(*t.params,t.input[i:i+1],1,1,state,t.output[:1])
        return np.asarray(states)

    def check_arc(self,left,right,nl,nr,radius):
        x0,x1=left[:,:14],right[:,:14]
        lo,hi=left[0,-1],right[0,-1]
        scale=self.scale
        delta=Ball(right)-Ball(left)
        segment=hull(np.minimum(left,right),np.maximum(left,right))
        tube=segment+Ball(0.,(Ball(scale)*radius).magnitude())
        u=constant(self.input)
        def evaluate(box,derivatives,dk):
            k=Ball(box.m[:,-1],box.r[:,-1])
            h=(self.critical-k).inv()
            c=self.coeff.copy()
            c[1],c[8]=k,h*h+Ball(0.,2**-48)
            z=[Jet(Ball(box.m[:,i],box.r[:,i]),derivatives[i]) for i in range(14)]
            states,*_=advance(z,u,c,dk,(2.*h*h*h).col()*dk)
            return states
        eye=np.eye(15)
        states=evaluate(tube,[Ball(row) for row in eye[:14]],Ball(eye[-1]))
        physical=stack([x.d for x in states])*(Ball(scale[None,:])/Ball(scale[:14,None]))
        # K propagates unchanged except at sample zero, where the fixed-point
        # equation substitutes the interpolated hyperplane for its redundant wrap condition.
        jm=np.zeros((len(left),15,15));jr=np.zeros_like(jm)
        jm[:,:14,:],jr[:,:14,:]=physical.m,physical.r
        jm[:,14,14]=1.
        row=Ball(eye[-1])-hull(np.minimum(nl,nr),np.maximum(nl,nr))
        jm[0,14,:],jr[0,14,:]=row.m,row.r
        directed=[Ball(delta.m[:,i,None],delta.r[:,i,None]) for i in range(15)]
        deriv=stack([s.d for s in evaluate(segment,directed,directed[-1])])
        slope=(Ball(deriv.m[:,:,0],deriv.r[:,:,0])-Ball(np.roll(delta.m[:,:14],-1,axis=0),np.roll(delta.r[:,:14],-1,axis=0))).magnitude()
        edge=[]
        for x in (left,right):
            states=evaluate(Ball(x,np.zeros_like(x)),[Ball(0.)]*14,Ball(0.))
            edge.append((stack([w.v for w in states])-Ball(np.roll(x[:,:14],-1,axis=0))).magnitude())
        defect=np.zeros((len(left),15))
        defect[:,:14]=(np.maximum(*edge)+.5*slope+TINY)*(1+16*EPS)/scale[:14]
        err=jr.sum(axis=2)*(1+32*EPS)
        eta,lip=self.inverse(jm,np.stack((defect*(1+EPS),err),axis=-1))
        return (Ball(lip)*radius+eta).hi()<radius,float(lip),float(eta)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('arcs',type=Path)
    ap.add_argument('out',type=Path)
    ap.add_argument('--q',type=int,default=190)
    ap.add_argument('--limit',type=int)
    ap.add_argument('--begin',type=int,default=0)
    ap.add_argument('--segmented',action='store_true')
    ap.add_argument('--first-turn',action='store_true',help='Validate the saved arc from zero through its first maximum')
    ap.add_argument('--bridge',action='store_true',help='Continue between fixed-feedback sections with the stored endpoint orientations')
    args=ap.parse_args()
    data=next(x for x in map(json.loads,args.arcs.open()) if x['q']==args.q and
              (x['reason']=='bridge' if args.bridge else x['reason']=='first_turn' if args.first_turn else x['reason']=='target' and x['step']==.02))
    t=Tone(args.q,20*1000**(56/99),90/99)
    dry=2.**np.floor(np.log2(t.anchor[8]))
    scale=np.array([1.]*4+[np.sqrt(dry)/4]*4+[dry/8]+[1.]*4+[.5,1.])
    arc=Arc(t,scale)
    if args.segmented:
        from segmented_inverse import inverse_bounds
        arc.inverse=inverse_bounds
    proposals=[np.r_[r['state'],3.9*np.expm1(np.log(49)*r['r'])/48] for r in data['path']]
    if not (args.first_turn or args.bridge):
        proposals[-1]=np.r_[data['target']['state'],data['target']['base_feedback']]
    nodes=np.array(proposals)
    normals=np.zeros_like(nodes)
    normals[[0,-1],-1]=1.
    if args.first_turn or args.bridge:
        normals[-1,-1]=np.sign(data['path'][-1]['tangent_r'])
        normals[0,-1]=np.sign(data['path'][0]['tangent_r'])
    diff=(nodes[2:]-nodes[:-2])/scale
    normals[1:-1]=diff/np.linalg.norm(diff,axis=1)[:,None]
    left=arc.states(nodes[args.begin])
    saved=[left]
    cell_normals=[normals[args.begin]]
    with args.out.open('x',buffering=1) as file:
        print(json.dumps(dict(kind='setup',q=args.q,cutoff=t.cutoff,drive=t.drive,scale=scale.tolist(),
                             begin=args.begin,segmented=args.segmented)),file=file)
        def segment(x,y,a,b,depth):
            span=np.linalg.norm((x[0]-y[0])/scale)
            rad=max(1e-9,min(.001,span**2*.1))
            for _ in range(4):
                ok,lip,eta=arc.check_arc(x,y,a,b,rad)
                if ok:
                    print(json.dumps(dict(kind='cell',lo=x[0,-1],hi=y[0,-1],radius=rad,contraction=lip,defect=eta)),file=file)
                    saved.append(y); cell_normals.append(b)
                    return
                if lip>=.98: break
                rad=max(1.4*rad,1.15*eta/(1.-lip))
            if depth>=18:
                raise ArithmeticError(f"unresolved chart at K={x[0,-1]}, lip={lip}, eta={eta}")
            n=(a+b)*.5
            middle=arc.point(x,y,n)
            segment(x,middle,a,n,depth+1)
            segment(middle,y,n,b,depth+1)
        for i in range(args.begin+1,len(nodes) if args.limit is None else min(args.begin+args.limit+1,len(nodes))):
            right=arc.states(nodes[i])
            segment(left,right,normals[i-1],normals[i],0)
            left=right
    np.savez_compressed(args.out.with_suffix('.npz'),states=np.array(saved),normals=np.array(cell_normals))
    print('certified',len(saved)-1,'charts to',left[0,-1],flush=True)


if __name__=='__main__':
    main()
