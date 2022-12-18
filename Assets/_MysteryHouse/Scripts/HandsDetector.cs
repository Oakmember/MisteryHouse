using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class HandsDetector : MonoBehaviour
{
    // Start is called before the first frame update
    void Start()
    {
        
    }

    private void OnTriggerEnter(Collider other)
    {
        if (other.CompareTag(Consts.rightHandTag))
        {
            HandComplete handComplete = other.GetComponent<HandComplete>();
            if (handComplete)
            {
                handComplete.SetPressAnim(true);
            }
        }

        if (other.CompareTag(Consts.leftHandTag))
        {
            HandComplete handComplete = other.GetComponent<HandComplete>();
            if (handComplete)
            {
                handComplete.SetPressAnim(true);
            }
        }
    }

    private void OnTriggerExit(Collider other)
    {
        if (other.CompareTag(Consts.rightHandTag))
        {
            HandComplete handComplete = other.GetComponent<HandComplete>();
            if (handComplete)
            {
                handComplete.SetPressAnim(false);
            }
        }

        if (other.CompareTag(Consts.leftHandTag))
        {
            HandComplete handComplete = other.GetComponent<HandComplete>();
            if (handComplete)
            {
                handComplete.SetPressAnim(false);
            }
        }
    }
}
